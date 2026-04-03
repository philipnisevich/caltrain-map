# Hosting the Caltrain API (production)

This guide walks through running the FastAPI backend on a **Linux VPS** (e.g. **DigitalOcean Droplet**) so the ESP32 can reach it 24/7 without your laptop. It matches the setup we used: **Ubuntu 24.04**, **systemd**, **uvicorn** on `127.0.0.1:8000`, and **Caddy** on port **80** with a real domain.

Your firmware uses plain **HTTP** (`WiFiClient` in the sketch), so the examples use **`http://api.yourdomain.com/caltrain`**, not HTTPS.

---

## What you need

- A **511.org** transit API key ([511 Open Data](https://511.org/open-data/transit)).
- A **VPS** with a public IPv4 (this doc assumes Ubuntu 22.04/24.04).
- A **domain** and DNS control (e.g. `api.yourdomain.com` → Droplet IP). You can use your registrar or Cloudflare for DNS only.
- SSH access to the server (port **22**). Some corporate Wi‑Fi blocks outbound SSH; use home Wi‑Fi or a hotspot if `ssh` times out.

---

## 1. Create the Droplet

1. Create an Ubuntu **LTS** Droplet (1 GB RAM is enough).
2. Add your **SSH public key** when creating the instance. Use the **`.pub`** file (one line starting with `ssh-ed25519` or `ssh-rsa`), not the private key.
3. Note the **public IPv4** address.

**First login** (from your Mac):

```bash
ssh root@YOUR_DROPLET_IP
```

---

## 2. Firewall (UFW)

On the server:

```bash
apt update && apt upgrade -y
ufw allow OpenSSH
ufw allow 80/tcp
ufw allow 443/tcp
ufw enable
```

If you use a **DigitalOcean Cloud Firewall**, also allow **TCP 22** there; otherwise SSH can time out from your laptop.

---

## 3. Install Python and clone the app

```bash
apt install -y python3 python3-venv python3-pip git curl
mkdir -p /opt/caltrain-api
cd /opt/caltrain-api
git clone https://github.com/YOUR_USER/YOUR_REPO.git .
```

If you clone into a subfolder by mistake, either move files up to `/opt/caltrain-api` or set `WorkingDirectory` in systemd to that folder.

**Virtualenv and dependencies:**

```bash
cd /opt/caltrain-api
python3 -m venv .venv
./.venv/bin/pip install --upgrade pip
./.venv/bin/pip install -r requirements.txt
```

The **`gtfs-realtime-bindings`** package is required for `from google.transit import gtfs_realtime_pb2`.

---

## 4. API key (systemd environment file)

Create a root-readable env file **only** on the server (never commit this):

```bash
nano /etc/caltrain-api.env
```

Put a single line (no `export`, no spaces around `=`):

```env
API_KEY=your_511_api_key_here
```

Save, then:

```bash
chmod 600 /etc/caltrain-api.env
```

**Common mistake:** pasting shell commands like `chmod 600 ...` *inside* the env file. The file must contain only `KEY=value` lines.

---

## 5. systemd service

Create `/etc/systemd/system/caltrain-api.service`:

```ini
[Unit]
Description=Caltrain API (uvicorn)
After=network.target

[Service]
Type=simple
User=root
WorkingDirectory=/opt/caltrain-api
EnvironmentFile=/etc/caltrain-api.env
ExecStart=/opt/caltrain-api/.venv/bin/uvicorn main:app --host 127.0.0.1 --port 8000
Restart=always

[Install]
WantedBy=multi-user.target
```

Bind to **`127.0.0.1`** so the app is only reachable on the machine; **Caddy** will expose it on port 80.

Enable and start:

```bash
systemctl daemon-reload
systemctl enable --now caltrain-api
systemctl status caltrain-api --no-pager
```

**Check locally on the server** (wait a second after restart if `curl` fails immediately):

```bash
sleep 2
curl -sS http://127.0.0.1:8000/caltrain | head
```

**Logs:**

```bash
journalctl -u caltrain-api -n 50 --no-pager -l
```

If you see `ModuleNotFoundError: No module named 'google.transit'`, install **`gtfs-realtime-bindings`** in the same venv systemd uses. If you see `Missing API_KEY`, fix `/etc/caltrain-api.env` and restart the service.

---

## 6. DNS

Add an **A** record:

| Type | Name | Value        |
|------|------|--------------|
| A    | api  | Droplet IPv4 |

So **`api.yourdomain.com`** resolves to your server. Verify from your Mac:

```bash
dig +short api.yourdomain.com
```

---

## 7. Caddy (reverse proxy on port 80)

Install Caddy using the **official** instructions for Debian/Ubuntu:

https://caddyserver.com/docs/install#debian-ubuntu-raspbian  

(Run the full block: key, apt source, `apt update`, `apt install caddy`.)

Replace **`/etc/caddy/Caddyfile`** with:

```caddyfile
http://api.yourdomain.com {
    reverse_proxy 127.0.0.1:8000
}
```

Use your real hostname. The **`http://`** prefix keeps this site on **HTTP only**, which matches the ESP32 sketch’s **`WiFiClient`** (no TLS).

Validate and reload:

```bash
caddy validate --config /etc/caddy/Caddyfile
systemctl reload caddy
```

**Test on the server** (Host header matches what browsers and your domain send):

```bash
curl -sS -H "Host: api.yourdomain.com" http://127.0.0.1/caltrain | head
```

**Test from your Mac:**

```bash
curl -sS http://api.yourdomain.com/caltrain | head
```

---

## 8. Firmware

Copy `caltrain_firmware/server_config.example.h` to `caltrain_firmware/server_config.h` and set:

```cpp
const char* serverUrl = "http://api.yourdomain.com/caltrain";
```

Flash the ESP32 and watch the serial monitor (115200 baud).

---

## 9. Updating the app

```bash
cd /opt/caltrain-api
git pull
./.venv/bin/pip install -r requirements.txt   # if dependencies changed
systemctl restart caltrain-api
```

---

## Optional: HTTPS later

Browsers and Let’s Encrypt are happy with **`https://api.yourdomain.com`**, but the current firmware uses **`WiFiClient`**, not **`WiFiClientSecure`**. To use HTTPS you would change the sketch to use TLS and a proper `https://` URL (and handle certificate validation). Until then, stay on **HTTP** behind your hostname as above.

---

## Cloudflare

You do **not** need Cloudflare to run the API. Use it only if you want it for **DNS** (or CDN/proxy). If the hostname is **DNS-only** (gray cloud), Caddy on your Droplet terminates HTTP as described. If you **proxy** (orange cloud), review SSL/TLS mode so it still works with your ESP HTTP client.

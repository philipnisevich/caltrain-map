# Run modes (local, cloud, Raspberry Pi)

This repo’s backend is a small **FastAPI** service (`GET /caltrain`) that the ESP32 polls over **HTTP**.

You can run it three common ways:

- **Local (Mac/Linux PC)**: easiest for development, requires your computer to stay on.
- **Cloud VPS**: public, stable, runs 24/7 (see `HOSTING.md`).
- **Raspberry Pi**: runs 24/7 at home on your LAN.

The firmware URL lives in `caltrain_firmware/server_config.h` (git-ignored). The sketch uses **HTTP** (`WiFiClient`), not HTTPS.

---

## Common prerequisites (all modes)

- **511 API key** and `API_KEY` available to the backend.
- Python dependencies installed from `requirements.txt`.

From the repo root:

```bash
python3 -m venv .venv
source .venv/bin/activate
pip install --upgrade pip
pip install -r requirements.txt
```

Backend API contract (used by firmware):

- `GET /caltrain` → JSON `{ "timestamp": ..., "trains": [ { "station": "...", "direction": "NORTH|SOUTH" } ] }`

---

## Mode 1: Run locally on your computer (development)

### When to use

- You’re iterating on code and want the simplest workflow.

### Run

```bash
source .venv/bin/activate
uvicorn main:app --host 0.0.0.0 --port 8000
```

### Firmware `serverUrl`

- Put your computer’s **LAN IP**:

```cpp
const char* serverUrl = "http://YOUR_MAC_LAN_IP:8000/caltrain";
```

On macOS, get the LAN IP with:

```bash
ipconfig getifaddr en0
```

### Pros / cons

- **Pros**: fastest to change and test.
- **Cons**: computer must stay on; IP can change with DHCP.

---

## Mode 2: Run on a cloud VPS (24/7, public)

### When to use

- You want the API reachable 24/7 without relying on your home network.

### Summary

This is the setup documented in **`HOSTING.md`**:

- Code under `/opt/caltrain-api`
- Python venv under `/opt/caltrain-api/.venv`
- `API_KEY` in `/etc/caltrain-api.env`
- `systemd` service runs uvicorn on `127.0.0.1:8000`
- **Caddy** listens on port 80 and reverse-proxies `http://api.yourdomain.com` → `127.0.0.1:8000`

### Firmware `serverUrl`

```cpp
const char* serverUrl = "http://api.yourdomain.com/caltrain";
```

### Pros / cons

- **Pros**: stable public endpoint; doesn’t depend on home Wi‑Fi.
- **Cons**: monthly cost; basic server maintenance (updates, backups).

---

## Mode 3: Run on a Raspberry Pi (24/7 on your LAN)

### When to use

- You want “always on” without paying for a VPS and you’re OK with home networking.

### Recommended OS settings

- Use **Raspberry Pi OS Lite** (no desktop).
- Enable **SSH** and configure **Wi‑Fi** in Raspberry Pi Imager.

### Install and run (high-level)

1. SSH into the Pi.
2. Install packages:

```bash
sudo apt update
sudo apt install -y python3 python3-venv python3-pip git
```

3. Deploy the repo (git clone or `rsync`) into `/opt/caltrain-api`.
4. Create venv + install:

```bash
cd /opt/caltrain-api
python3 -m venv .venv
./.venv/bin/pip install --upgrade pip
./.venv/bin/pip install -r requirements.txt
```

5. Provide `API_KEY` (either a user-readable env file, or follow the `HOSTING.md` pattern with `/etc/caltrain-api.env` and `systemd`).
6. Run `uvicorn` with `--host 0.0.0.0 --port 8000` so the ESP32 can reach the Pi over your LAN.

### Firmware `serverUrl`

Use the Pi’s LAN IP (recommended: reserve it in your router DHCP settings):

```cpp
const char* serverUrl = "http://PI_LAN_IP:8000/caltrain";
```

### Pros / cons

- **Pros**: no monthly VPS cost; stays local; low power.
- **Cons**: IP changes unless reserved; availability depends on home power/Wi‑Fi; remote access requires port forwarding/VPN/tunnel.


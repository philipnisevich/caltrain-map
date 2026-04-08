# Caltrain LED Map

A live Caltrain tracker that:

- runs a **FastAPI** backend (`GET /caltrain`) that pulls Caltrain vehicle positions from **511 GTFS-Realtime**,
- maps trains to station codes using `stations.py`,
- and lights LEDs on an **ESP32** + **MAX72xx** matrix.

You can run the backend on your **Mac** for development or on a **VPS** 24/7 for production. See **[HOSTING.md](HOSTING.md)** for the full DigitalOcean + systemd + Caddy setup.
For a high-level comparison of local vs cloud vs Raspberry Pi, see **[RUN_MODES.md](RUN_MODES.md)**.

## Project structure

- `main.py` — FastAPI app (`/caltrain`) that fetches and transforms live train data.
- `stations.py` — Station coordinates / codes for the backend.
- `requirements.txt` — Python dependencies (use for local venv and production deploys).
- `caltrain_firmware/caltrain_firmware.ino` — ESP32 firmware (JSON → LEDs).
- `caltrain_firmware/server_config.h` — Local Wi-Fi + server URL (git-ignored; create from the example).
- `caltrain_firmware/server_config.example.h` — Template copied to `server_config.h`.
- `.env` — Local backend secrets (git-ignored).
- `.env.example` — Template for `API_KEY`.

## Security / secrets

Keep credentials out of Git:

- Backend: `.env` (local) or `/etc/caltrain-api.env` (server).
- Firmware: `caltrain_firmware/server_config.h`.

Commit only `*.example` templates.

## Prerequisites

### Backend

- Python 3.9+
- A [511.org](https://511.org/open-data/transit) API key
- Packages: install with `pip install -r requirements.txt` (includes `gtfs-realtime-bindings`, `fastapi`, `uvicorn`, etc.)

### Hardware / firmware

- ESP32, MAX72xx module
- Arduino IDE: ESP32 board support, **ArduinoJson**, **MD_MAX72xx**

## One-time setup (local / Mac)

### 1) Backend `.env`

```bash
cp .env.example .env
```

Edit `.env`:

```env
API_KEY=your_511_api_key_here
```

### 2) Firmware `server_config.h`

```bash
cp caltrain_firmware/server_config.example.h caltrain_firmware/server_config.h
```

Edit `caltrain_firmware/server_config.h`:

- **Local dev** (ESP and Mac on same Wi-Fi):

  ```cpp
  const char* serverUrl = "http://YOUR_MAC_LAN_IP:8000/caltrain";
  ```

- **Production** (API on a VPS with a domain):

  ```cpp
  const char* serverUrl = "http://api.yourdomain.com/caltrain";
  ```

Set `ssid` and `password` to your Wi-Fi. The sketch uses **HTTP** (`WiFiClient`), not HTTPS.

## Run the backend locally

From the repo root:

```bash
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
uvicorn main:app --host 0.0.0.0 --port 8000
```

Use `0.0.0.0` so devices on your LAN (e.g. the ESP32) can reach your Mac.

## Host the API in production

See **[HOSTING.md](HOSTING.md)** for:

- Ubuntu Droplet, UFW, `git clone` under `/opt/caltrain-api`
- **`systemd`** + **`/etc/caltrain-api.env`** for `API_KEY`
- **Caddy** on port 80 → `http://api.yourdomain.com` → uvicorn on `127.0.0.1:8000`
- DNS **A** record for `api`

## Find your Mac LAN IP

```bash
ipconfig getifaddr en0
```

Use that in `serverUrl` for local development.

## Verify the backend

```bash
curl http://127.0.0.1:8000/caltrain
```

Expect JSON with `timestamp` and `trains`.

## Upload firmware

1. Valid `API_KEY` in `.env` (local) or on the server (production).
2. `caltrain_firmware/server_config.h` has correct `ssid`, `password`, and `serverUrl`.
3. Open `caltrain_firmware/caltrain_firmware.ino` in Arduino IDE, select board/port, build and upload.
4. Serial Monitor at **115200** for Wi-Fi and HTTP logs.

## Runtime behavior

- ESP32 connects to Wi-Fi, then polls `serverUrl` about every 30 seconds.
- Backend returns station codes and `NORTH` / `SOUTH`.
- Firmware maps codes to LED positions on the matrix.

## API contract (`GET /caltrain`)

```json
{
  "timestamp": 1234567890,
  "trains": [
    { "station": "SF", "direction": "NORTH" }
  ]
}
```

## Common issues

- **ESP cannot reach the backend (local)**  
  Same Wi-Fi, Mac firewall, `uvicorn --host 0.0.0.0`, correct LAN IP in `serverUrl`.

- **ESP cannot reach the backend (production)**  
  DNS for `api`, Caddy running, `caltrain-api` active, URL is `http://` (not `https://`) with current firmware.

- **`Missing API_KEY`**  
  `.env` locally or `/etc/caltrain-api.env` on the server; `EnvironmentFile=` in systemd.

- **`No module named 'google.transit'`**  
  `pip install gtfs-realtime-bindings` in the same venv that runs uvicorn.

- **No trains**  
  511 sometimes returns empty data; check backend logs and Serial Monitor.

## Git hygiene

Ignored paths (see `.gitignore`): `.env`, `venv/`, `.venv/`, `**/server_config.h`, etc.

Commit source and `*.example` files only.

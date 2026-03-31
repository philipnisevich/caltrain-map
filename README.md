# Caltrain LED Map

A live Caltrain tracker that:
- runs a FastAPI backend on your Mac,
- pulls Caltrain vehicle positions from 511 GTFS-Realtime,
- maps trains to station codes,
- and lights LEDs on an ESP32 + MAX72xx matrix.

## Project Structure

- `main.py` - FastAPI app (`/caltrain`) that fetches and transforms live train data.
- `stations.py` - station coordinate/code mapping used by the backend.
- `Caltrain_Firmware/Caltrain_Firmware.ino` - ESP32 firmware that fetches backend JSON and updates LEDs.
- `Caltrain_Firmware/wifi_secrets.h` - local Wi-Fi credentials (git-ignored).
- `Caltrain_Firmware/wifi_secrets.example.h` - template for Wi-Fi credentials.
- `Caltrain_Firmware/server_config.h` - local backend URL config (git-ignored).
- `Caltrain_Firmware/server_config.example.h` - template for backend URL config.
- `.env` - local backend API secrets (git-ignored).
- `.env.example` - template env file for backend.

## Security / Secrets

Sensitive values are intentionally split into local files that are ignored by Git:

- Backend API key: `.env`
- ESP32 Wi-Fi: `Caltrain_Firmware/wifi_secrets.h`
- ESP32 server URL / local IP: `Caltrain_Firmware/server_config.h`

Do **not** commit those files. Commit only the `*.example` template files.

## Prerequisites

### Backend (Mac)
- Python 3.9+
- A 511.org API key
- Python packages used by `main.py`:
  - `fastapi`
  - `uvicorn`
  - `requests`
  - `gtfs-realtime-bindings`
  - `geopy`

### Hardware / Firmware
- ESP32 board
- MAX72xx LED matrix module
- Arduino IDE with:
  - ESP32 board package
  - Libraries:
    - `ArduinoJson`
    - `MD_MAX72xx`

## One-Time Setup

### 1) Backend secrets
Create `.env` in repo root:

```bash
cp .env.example .env
```

Then edit `.env`:

```env
API_KEY=your_511_api_key_here
```

### 2) Firmware local config
Create local secret/config headers:

```bash
cp Caltrain_Firmware/wifi_secrets.example.h Caltrain_Firmware/wifi_secrets.h
cp Caltrain_Firmware/server_config.example.h Caltrain_Firmware/server_config.h
```

Edit `Caltrain_Firmware/wifi_secrets.h`:

```cpp
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";
```

Edit `Caltrain_Firmware/server_config.h` with your Mac's LAN IP:

```cpp
const char* serverUrl = "http://YOUR_MAC_IP:8000/caltrain";
```

## Running the Backend

From project root:

```bash
source venv/bin/activate
uvicorn main:app --host 0.0.0.0 --port 8000
```

Why `0.0.0.0`?  
It allows your ESP32 (on the same network) to reach your Mac.

## Find Your Mac LAN IP

Use:

```bash
ipconfig getifaddr en0
```

Put that IP into `server_config.h`.

## Verify Backend Before Flashing

With server running:

```bash
curl http://127.0.0.1:8000/caltrain
curl http://YOUR_MAC_IP:8000/caltrain
```

Both should return JSON with a `timestamp` and `trains` array.

## Uploading Firmware

1. Open `Caltrain_Firmware/Caltrain_Firmware.ino` in Arduino IDE.
2. Select your ESP32 board and correct serial port.
3. Install required libraries if missing.
4. Build and upload.
5. Open Serial Monitor (`115200`) to watch Wi-Fi and HTTP logs.

## Runtime Behavior

- ESP32 connects to Wi-Fi using `wifi_secrets.h`.
- Every ~30 seconds it calls `serverUrl`.
- Backend returns train station codes + direction (`NORTH` / `SOUTH`).
- Firmware maps station code -> LED position and updates matrix.

## API Contract Used by Firmware

Endpoint:

- `GET /caltrain`

Response shape:

```json
{
  "timestamp": 1234567890,
  "trains": [
    { "station": "SF", "direction": "NORTH" }
  ]
}
```

## Common Issues

- **ESP32 cannot connect to backend**
  - Mac and ESP32 must be on same Wi-Fi.
  - Backend must run with `--host 0.0.0.0`.
  - Check Mac firewall allows incoming Python/Terminal.
  - Verify `serverUrl` IP is current (it may change with DHCP).

- **Missing `API_KEY` error in backend**
  - Ensure `.env` exists and contains `API_KEY=...`.

- **No trains displayed**
  - 511 API can return no data at times.
  - Check backend logs and Serial Monitor output.

## Git Hygiene

Ignored local-only files:
- `.env`
- `venv/`, `.venv/`
- `.DS_Store`
- `test.pb`
- `Caltrain_Firmware/wifi_secrets.h`
- `Caltrain_Firmware/server_config.h`

Recommended workflow:
- commit source and `*.example` files,
- keep real credentials only in local ignored files.


// Copy this file to server_config.h and fill in all values.
// server_config.h is git-ignored to keep credentials/private network config private.

const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";
// Production (VPS + Caddy): http://api.yourdomain.com/caltrain
// Local dev (Mac + same Wi-Fi): http://YOUR_MAC_LAN_IP:8000/caltrain
const char* serverUrl = "http://api.yourdomain.com/caltrain";

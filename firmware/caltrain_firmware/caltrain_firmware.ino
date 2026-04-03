#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <MD_MAX72xx.h>
#include <SPI.h>
#include "server_config.h"

// ===================== CONFIG =====================

#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define CS_PIN 5
#define MAX_DEVICES 1
#define ROTATE_DISPLAY_90_CCW 1  // set to 1 to rotate output 90 degrees CCW

MD_MAX72XX mx = MD_MAX72XX(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);

enum Direction { NORTH, SOUTH };

// ===================== STATION MAP =====================
struct Station {
  const char* name;
  uint8_t row;
  uint8_t col;   // NORTH = col, SOUTH = col+1
};

Station stations[] = {
  // index 0–3
  {"San Francisco",        0, 0},  // SF
  {"22nd Street",          0, 2},  // 22ND
  {"Bayshore",             0, 4},  // BAYS
  {"South San Francisco",  0, 6},  // SSAN

  // index 4–7
  {"San Bruno",            1, 0},  // SBRU
  {"Millbrae",             1, 2},  // MILL
  {"Broadway",             1, 4},  // BWAY
  {"Burlingame",           1, 6},  // BURL

  // index 8–11
  {"San Mateo",            2, 0},  // SMAT
  {"Hayward Park",         2, 2},  // HWDP
  {"Hillsdale",            2, 4},  // HSDL
  {"Belmont",              2, 6},  // BLMT

  // index 12–15
  {"San Carlos",           3, 0},  // BLMT (San Carlos)
  {"Redwood City",         3, 2},  // SACL / RWC
  {"Menlo Park",           3, 4},  // MPK
  {"Palo Alto",            3, 6},  // PA

  // index 16–19
  {"California Ave",       4, 0},  // CAAV
  {"San Antonio",          4, 2},  // SANT
  {"Mountain View",        4, 4},  // MTV
  {"Sunnyvale",            4, 6},  // SUNN

  // index 20–23
  {"Lawrence",             5, 0},  // LAW
  {"Santa Clara",          5, 2},  // SCL
  {"College Park",         5, 4},  // COL
  {"San Jose Diridon",     5, 6},  // SJ

  // index 24–27
  {"Tamien",               6, 0},  // TAM
  {"Capitol",              6, 2},  // CAP
  {"Blossom Hill",         6, 4},  // BHL
  {"Morgan Hill",          6, 6},  // MHILL

  // index 28–29
  {"San Martin",           7, 0},  // SMTN
  {"Gilroy",               7, 2}   // GIL
};


const int NUM_STATIONS = sizeof(stations)/sizeof(stations[0]);

// ===================== SERVER CODE MAPPING =====================
struct StationCodeMap {
  const char* code;
  int stationIndex;
};

StationCodeMap codeMap[] = {
  // ================= REAL STATIONS =================
  {"SF", 0},          // San Francisco
  {"22ND", 1},        // 22nd Street
  {"BAYS", 2},        // Bayshore
  {"SSAN", 3},        // South San Francisco
  {"SBRU", 4},        // San Bruno
  {"MILL", 5},        // Millbrae
  {"BWAY", 6},        // Broadway
  {"BURL", 7},        // Burlingame
  {"SMAT", 8},        // San Mateo
  {"HWDP", 9},        // Hayward Park
  {"HSDL", 10},        // Belmont
  {"BLMT", 11},       // San Carlos
  {"SACL", 12},       // Redwood City
  {"RWC", 13},        // Redwood City duplicate? map to same index
  {"MPK", 14},        // Menlo Park
  {"PA", 15},         // Palo Alto
  {"CAAV", 16},       // California Ave
  {"SANT", 17},       // San Antonio
  {"MTV", 18},        // Mountain View
  {"SUNN", 19},       // Sunnyvale
  {"LAW", 20},        // Lawrence
  {"SCL", 21},        // Santa Clara
  {"COL", 22},        // College Park
  {"SJ", 23},         // San Jose Diridon
  {"TAM", 24},        // Tamien
  {"CAP", 25},        // Capitol
  {"BHL", 26},        // Blossom Hill
  {"MHILL", 27},      // Morgan Hill
  {"SMTN", 28},       // San Martin
  {"GIL", 29}         // Gilroy
};

const int NUM_CODES = sizeof(codeMap)/sizeof(codeMap[0]);

int findStationIndexFromCode(const char* code) {
  for (int i = 0; i < NUM_CODES; i++) {
    if (strcmp(code, codeMap[i].code) == 0) return codeMap[i].stationIndex;
  }
  return -1;
}

// ===================== HELPERS =====================
void setStationLED(int idx, Direction dir) {
  uint8_t row = stations[idx].row;
  uint8_t col = stations[idx].col + (dir == SOUTH ? 1 : 0);
#if ROTATE_DISPLAY_90_CCW
  // 8x8 CCW rotation: (x, y) -> (y, 7 - x)
  mx.setPoint(row, 7 - col, true);
#else
  mx.setPoint(col, row, true);
#endif
}

// ===================== FETCH & UPDATE =====================
void updateFromServer() {
  WiFiClient client;
  HTTPClient http;

  Serial.println("Requesting Caltrain data...");

  if (!http.begin(client, serverUrl)) {
    Serial.println("HTTP begin failed");
    return;
  }

  int httpCode = http.GET();

  if (httpCode <= 0) {
    Serial.print("HTTP request failed, error: ");
    Serial.println(http.errorToString(httpCode));
    http.end();
    return;
  }

  Serial.print("HTTP code: ");
  Serial.println(httpCode);

  String payload = http.getString();
  http.end();

  Serial.println("Payload received:");
  Serial.println(payload);

  mx.clear();

  // ===================== PARSE JSON =====================
  StaticJsonDocument<1024> doc;
  DeserializationError error = deserializeJson(doc, payload);

  if (error) {
    Serial.print("JSON parse failed: ");
    Serial.println(error.c_str());
    return;
  }

  JsonArray trains = doc["trains"].as<JsonArray>();

  Serial.println("----- LIVE DATA UPDATE -----");

  for (JsonObject train : trains) {
    const char* stationCode = train["station"];
    const char* dirStr = train["direction"];

    int idx = findStationIndexFromCode(stationCode);
    if (idx < 0) {
      Serial.print("Unknown station code: ");
      Serial.println(stationCode);
      continue;
    }

    Direction dir = (strcmp(dirStr, "NORTH") == 0) ? NORTH : SOUTH;
    setStationLED(idx, dir);

    Serial.print("LIVE -> ");
    Serial.print(stations[idx].name);
    Serial.print(" | ");
    Serial.println(dirStr);
  }

  Serial.println("----------------------------");
}

// ===================== SETUP =====================
void setup() {
  Serial.begin(115200);
  delay(1000);

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected. IP: " + WiFi.localIP().toString());

  WiFi.setSleep(false);

  SPI.begin(18, -1, 23, CS_PIN);
  SPI.setFrequency(1000000);

  mx.begin();
  mx.control(MD_MAX72XX::INTENSITY, 5);
  mx.clear();

  updateFromServer();  // initial fetch
}

// ===================== LOOP =====================
void loop() {
  static unsigned long lastUpdate = 0;
  unsigned long now = millis();

  if (now - lastUpdate >= 30000) {  // update every 30 seconds
    lastUpdate = now;
    updateFromServer();
  }
}

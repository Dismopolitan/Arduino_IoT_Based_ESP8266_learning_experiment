#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <ArduinoJson.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoOTA.h>

#define EEPROM_SIZE 256  // Ensure enough space for Config

// Fallback AP
#define APSSID "ESPap"
#define APPSK  "thereisnospoon"

// Cloud server
const char* cloudURL = "*************"; // Local Python server
unsigned long lastPost = 0;
const unsigned long postInterval = 60000; // 60 seconds

ESP8266WebServer server(80);

// Wi-Fi reconnect timer
unsigned long lastWifiCheck = 0;
const unsigned long wifiCheckInterval = 15000; // 15 seconds

// ==========================
// Configuration struct
// ==========================
struct Config {
  uint32_t magic;       // detects first-time use / invalid EEPROM
  uint16_t ver;         // config version
  char deviceName[32];
  char wifiSSID[64];
  char wifiPass[64];
};

Config config;

// ==========================
// EEPROM Functions
// ==========================
void saveConfig() {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.put(0, config);
  EEPROM.commit();
  EEPROM.end();
}

void loadConfig() {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.get(0, config);
  EEPROM.end();

  if (config.magic != 0xC0FFEE01 || config.ver != 1) {
    memset(&config, 0, sizeof(config));
    config.magic = 0xC0FFEE01;
    config.ver = 1;

    strncpy(config.deviceName, "ESP_Node", sizeof(config.deviceName)-1);
    config.deviceName[sizeof(config.deviceName)-1] = '\0';

    strncpy(config.wifiSSID, "", sizeof(config.wifiSSID)-1);
    config.wifiSSID[sizeof(config.wifiSSID)-1] = '\0';

    strncpy(config.wifiPass, "", sizeof(config.wifiPass)-1);
    config.wifiPass[sizeof(config.wifiPass)-1] = '\0';

    saveConfig();
  }
}

// ==========================
// Wi-Fi Setup
// ==========================
void setupWiFi() {
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);
  WiFi.mode(WIFI_AP_STA); // STA + AP

  if (strlen(config.wifiSSID) > 0) {
    WiFi.begin(config.wifiSSID, config.wifiPass);
    Serial.print("Connecting to WiFi");
    int retries = 0;
    const int maxRetries = 20;
    while (WiFi.status() != WL_CONNECTED && retries < maxRetries) {
      delay(500);
      Serial.print(".");
      retries++;
    }
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nConnected! IP: " + WiFi.localIP().toString());
    } else {
      Serial.println("\nFailed to connect. Starting AP mode...");
      WiFi.softAP(config.deviceName, "config123");
      Serial.println("AP IP: " + WiFi.softAPIP().toString());
    }
  } else {
    Serial.println("No Wi-Fi credentials. Starting AP mode...");
    WiFi.softAP(config.deviceName, "config123");
    Serial.println("AP IP: " + WiFi.softAPIP().toString());
  }
}

// ==========================
// Web Handlers
// ==========================
void handleRoot() {
  String html = "<html><head><title>ESP8266 Dashboard</title></head><body>";
  html += "<h1>ESP8266 Dashboard</h1>";
  html += "<p><b>Device Name:</b> " + String(config.deviceName) + "</p>";
  html += "<p><b>Wi-Fi:</b> " + String(config.wifiSSID) + "</p>";

  // Placeholders for sensor values
  html += "<h2>Sensor Data (auto-updating)</h2>";
  html += "<ul>";
  html += "<li>Temperature: <span id='temperature'>--</span> °C</li>";
  html += "<li>Humidity: <span id='humidity'>--</span> %</li>";
  html += "<li>Battery: <span id='battery'>--</span> V</li>";
  html += "<li>Counter: <span id='counter'>--</span> s</li>";
  html += "<li>RSSI: <span id='rssi'>--</span> dBm</li>";
  html += "</ul>";

  // Forms
  html += "<form action='/saveDeviceName' method='POST'>";
  html += "Set device name: <input name='deviceName' maxlength='31'>";
  html += "<input type='submit' value='Save'></form><hr>";
  html += "<form action='/saveWiFi' method='POST'>";
  html += "SSID: <input name='ssid' maxlength='63'><br>";
  html += "Password: <input name='pass' maxlength='63'><br>";
  html += "<input type='submit' value='Save Wi-Fi'></form><hr>";
  html += "<p><a href='/api/status'>View API Status (JSON)</a></p>";

  // JavaScript to auto-update sensor values every 2 seconds
  html += "<script>";
  html += "function updateSensors() {";
  html += "  fetch('/api/status').then(res=>res.json()).then(data=>{";
  html += "    document.getElementById('temperature').innerText = data.temperature;";
  html += "    document.getElementById('humidity').innerText = data.humidity;";
  html += "    document.getElementById('battery').innerText = data.battery;";
  html += "    document.getElementById('counter').innerText = data.counter;";
  html += "    document.getElementById('rssi').innerText = data.rssi;";
  html += "  });";
  html += "}";
  html += "setInterval(updateSensors, 2000);"; // every 2 seconds
  html += "updateSensors();"; // initial load
  html += "</script>";

  html += "</body></html>";

  server.send(200, "text/html", html);
}

void handleSaveDeviceName() {
  if (server.hasArg("deviceName")) {
    strncpy(config.deviceName, server.arg("deviceName").c_str(), sizeof(config.deviceName)-1);
    config.deviceName[sizeof(config.deviceName)-1] = '\0';
  }
  saveConfig();
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleSaveWiFi() {
  if (server.hasArg("ssid")) strncpy(config.wifiSSID, server.arg("ssid").c_str(), sizeof(config.wifiSSID)-1);
  if (server.hasArg("pass")) strncpy(config.wifiPass, server.arg("pass").c_str(), sizeof(config.wifiPass)-1);
  saveConfig();

  Serial.println("Attempting to connect to new Wi-Fi...");
  WiFi.disconnect();
  WiFi.begin(config.wifiSSID, config.wifiPass);

  unsigned long start = millis();
  const unsigned long timeout = 10000;
  bool connected = false;
  while (millis() - start < timeout) {
    if (WiFi.status() == WL_CONNECTED) {
      connected = true;
      break;
    }
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  String html = "<html><body>";
  html += "<h2>Wi-Fi Configuration Result</h2>";
  if (connected) {
    html += "<p>Connected successfully!</p>";
    html += "<p>STA IP Address: " + WiFi.localIP().toString() + "</p>";
  } else {
    html += "<p>Failed to connect. Starting fallback AP...</p>";
    WiFi.softAP(config.deviceName, "config123");
    html += "<p>AP IP: " + WiFi.softAPIP().toString() + "</p>";
  }
  html += "<p><a href='/'>Back to Dashboard</a></p>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

// ==========================
// REST API Endpoints
// ==========================
void handleApiStatus() {
  DynamicJsonDocument doc(512);
  doc["device"] = config.deviceName;
  doc["uptime"] = millis() / 1000;
  doc["wifi"] = config.wifiSSID;
  doc["ip"] = "hidden";  // hide local IP from public view
  doc["rssi"] = (WiFi.status() == WL_CONNECTED) ? WiFi.RSSI() : 0;

  // --- virtual sensors ---
  doc["temperature"] = 20.0 + random(0, 100) / 10.0;  // 20–30°C
  doc["humidity"]    = 40 + random(0, 60);            // 40–100%
  doc["battery"]     = 3.7 + random(0, 30)/100.0;     // 3.7–4.0V
  doc["counter"]     = millis() / 1000;

  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

void handleApiConfig() {
  DynamicJsonDocument doc(256);
  doc["device_name"] = config.deviceName;
  doc["wifi_ssid"] = config.wifiSSID;

  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

// ==========================
// Cloud HTTPS POST
// ==========================
void sendStatusToCloud() {
  if (WiFi.status() != WL_CONNECTED) return;
  if (millis() - lastPost < postInterval) return;

  const int maxRetries = 3;
  int attempt = 0;
  bool success = false;

  while (attempt < maxRetries && !success) {
    WiFiClientSecure client;
    client.setInsecure(); // For testing with self-signed cert

    HTTPClient https;
    if (https.begin(client, cloudURL)) {
      https.addHeader("Content-Type", "application/json");
      DynamicJsonDocument doc(512);
      doc["device"] = config.deviceName;
      doc["uptime"] = millis() / 1000;
      doc["wifi"] = config.wifiSSID;
      doc["ip"] = WiFi.localIP().toString();
      doc["rssi"] = WiFi.RSSI();

      // virtual sensors for cloud too
      doc["temperature"] = 20.0 + random(0, 100) / 10.0;
      doc["humidity"]    = 40 + random(0, 60);
      doc["battery"]     = 3.7 + random(0, 30)/100.0;
      doc["counter"]     = millis() / 1000;

      String payload;
      serializeJson(doc, payload);
      int httpCode = https.POST(payload);
      if (httpCode > 0) {
        Serial.printf("POST to cloud: %d\n", httpCode);
        String response = https.getString();
        Serial.println("Response: " + response);
        success = true;
      } else {
        Serial.printf("POST failed: %s\n", https.errorToString(httpCode).c_str());
        attempt++;
        delay(1000);
      }
      https.end();
    } else {
      Serial.println("HTTPS begin failed");
      attempt++;
      delay(1000);
    }
  }
  lastPost = millis();
}

// ==========================
// Setup & Loop
// ==========================
void setup() {
  Serial.begin(115200);
  delay(1000);

  loadConfig();
  setupWiFi();

  // OTA Setup
  ArduinoOTA.setHostname(config.deviceName);
  ArduinoOTA.setPassword("yourOTAPassword"); // optional
  ArduinoOTA.onStart([]() { Serial.println("Start OTA update"); });
  ArduinoOTA.onEnd([]() { Serial.println("\nEnd OTA update"); });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  Serial.println("OTA Ready");
  Serial.println(WiFi.localIP());

  // HTTP server setup
  server.on("/", handleRoot);
  server.on("/saveDeviceName", HTTP_POST, handleSaveDeviceName);
  server.on("/saveWiFi", HTTP_POST, handleSaveWiFi);
  server.on("/api/status", HTTP_GET, handleApiStatus);
  server.on("/api/config", HTTP_GET, handleApiConfig);
  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  server.handleClient();
  sendStatusToCloud();
  ArduinoOTA.handle();

  // Lightweight Wi-Fi reconnect
  if (millis() - lastWifiCheck > wifiCheckInterval) {
    lastWifiCheck = millis();
    if (WiFi.status() != WL_CONNECTED && strlen(config.wifiSSID) > 0) {
      Serial.println("Wi-Fi disconnected. Reconnecting...");
      WiFi.disconnect();
      WiFi.begin(config.wifiSSID, config.wifiPass);
    }
  }
}

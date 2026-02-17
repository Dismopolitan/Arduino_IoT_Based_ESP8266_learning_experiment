# ESP8266 IoT Node

This project is an ESP8266-based IoT firmware written using the Arduino framework for learning and experimentation purposes.

It demonstrates WiFi configuration handling, persistent storage, web-based device management, REST API design, HTTPS cloud communication, and OTA firmware updates.

---

## Overview

The firmware implements:

- Dual-mode WiFi (Station + Access Point fallback)
- EEPROM-based persistent configuration with versioning
- Embedded web dashboard
- JSON REST API
- Periodic HTTPS cloud reporting
- OTA firmware updates
- Automatic WiFi reconnection handling

Sensor values are currently simulated for testing.

---

## Hardware

- ESP8266 development board (NodeMCU, Wemos D1 Mini, or compatible)

---

## Software Requirements

### Arduino Board Package
- ESP8266 by ESP8266 Community

### Required Libraries

Install via Arduino Library Manager:

- ESP8266WiFi
- ESP8266WebServer
- EEPROM
- ArduinoJson
- ESP8266HTTPClient
- WiFiClientSecure
- ArduinoOTA

---

## Configuration Storage

Persistent configuration is stored in EEPROM using:

```cpp
struct Config {
  uint32_t magic;
  uint16_t ver;
  char deviceName[32];
  char wifiSSID[64];
  char wifiPass[64];
};
```

- Magic number: `0xC0FFEE01`
- Version: `1`

The magic number and version field allow detection of uninitialized or outdated configuration data.

---

## WiFi Behavior

If stored credentials exist:
- The device attempts Station (STA) connection.
- If connection fails, it falls back to Access Point (AP) mode.

If no credentials are stored:
- The device starts directly in AP mode.

Fallback AP:
- SSID: `<deviceName>`
- Password: `config123`

---

## Web Interface

The device hosts a web dashboard accessible at:

```
http://<device_ip>/
```

The dashboard allows:

- Viewing device information
- Viewing live sensor data (auto-updated via JavaScript)
- Changing device name
- Configuring WiFi credentials
- Viewing JSON API output

---

## REST API

### GET /api/status

Returns device status and sensor data in JSON format.

Example:

```json
{
  "device": "ESP_Node",
  "uptime": 1234,
  "wifi": "MyWiFi",
  "ip": "********",
  "rssi": -60,
  "temperature": 23.5,
  "humidity": 55,
  "battery": 3.82,
  "counter": 1234
}
```

---

### GET /api/config

Returns current configuration:

```json
{
  "device_name": "ESP_Node",
  "wifi_ssid": "MyWiFi"
}
```

---

## Cloud Integration

The firmware sends an HTTPS POST request at fixed intervals (default: 60 seconds) to a configured cloud endpoint.

- Retries up to 3 times on failure
- Includes device metadata and sensor data
- Uses WiFiClientSecure

Note:
`client.setInsecure()` is currently enabled for development/testing.

Cloud endpoint is defined in firmware:

```cpp
const char* cloudURL = "https://yourserver.com/endpoint";
```

---

## OTA Updates

OTA updates are enabled using ArduinoOTA.

- Hostname is set to the configured device name
- Password protection is supported

Firmware can be uploaded via:
Arduino IDE → Network Port

---

## Purpose

This project was developed as a learning exercise to explore:

- Embedded networking
- Persistent configuration design
- Web-based device management
- REST API structure
- Secure HTTP communication
- OTA update mechanisms
- Basic IoT architecture concepts

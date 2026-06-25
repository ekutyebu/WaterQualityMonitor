#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include "config.h"

// --- Global Variables ---
WebServer server(80);
unsigned long lastTelemetryTime = 0;
unsigned int intervalMinutes = DEFAULT_INTERVAL_MINS;

// Mock sensor states
float simTime = 0.0;
float currentTemp = 27.5;
float currentPh = 7.1;
float currentDo = 5.8; // dissolved oxygen in mg/L

// Remote control states synced from Next.js server
bool remoteAeratorActive = true;
bool remotePumpActive = false;

// --- Function Prototypes ---
void connectToWiFi();
void readSensors(float &temp, float &ph, float &doVal);
void runLocalAlarms(float temp, float ph, float doVal);
void sendTelemetry(float temp, float ph, float doVal);
void handleRoot();
void handleStatus();
void handleTriggerBuzzer();

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("==========================================");
  Serial.println("AquariumGuard: Starting Smart Aquaculture...");
  Serial.println("==========================================");

  // Initialize GPIO Pins
  pinMode(PIN_GREEN_LED, OUTPUT);
  pinMode(PIN_YELLOW_LED, OUTPUT);
  pinMode(PIN_RED_LED, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);

  // Turn off all indicators
  digitalWrite(PIN_GREEN_LED, LOW);
  digitalWrite(PIN_YELLOW_LED, LOW);
  digitalWrite(PIN_RED_LED, LOW);
  digitalWrite(PIN_BUZZER, LOW);

  // Connect to Local Wi-Fi
  connectToWiFi();

  // Set up local ESP32 Web Server Routes
  server.on("/", HTTP_GET, handleRoot);
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/trigger_buzzer", HTTP_GET, handleTriggerBuzzer);
  server.begin();
  Serial.println("Local HTTP Server started on port 80.");

  // Run initial telemetry post immediately
  float temp, ph, doVal;
  readSensors(temp, ph, doVal);
  sendTelemetry(temp, ph, doVal);
  lastTelemetryTime = millis();
}

void loop() {
  // Handle incoming HTTP client requests
  server.handleClient();

  // Check if it's time to send the next telemetry payload
  unsigned long now = millis();
  unsigned long intervalMs = intervalMinutes * 60 * 1000;

  if (now - lastTelemetryTime >= intervalMs || lastTelemetryTime == 0) {
    float temp, ph, doVal;
    readSensors(temp, ph, doVal);
    
    // Process local status indicators (Green, Yellow, Red, Buzzer)
    runLocalAlarms(temp, ph, doVal);

    // Upload to Next.js Local Server
    sendTelemetry(temp, ph, doVal);
    
    lastTelemetryTime = now;
  }

  // Brief loop yield
  delay(50);
}

// --- Wi-Fi Connection Manager ---
void connectToWiFi() {
  Serial.printf("Connecting to Local Wi-Fi SSID: '%s'...\n", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  unsigned long startAttempt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 15000) {
    // Pulse yellow LED while connecting
    digitalWrite(PIN_YELLOW_LED, HIGH);
    delay(250);
    digitalWrite(PIN_YELLOW_LED, LOW);
    delay(250);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWi-Fi Connected successfully!");
    Serial.print("Local ESP32 IP Address: ");
    Serial.println(WiFi.localIP());
    digitalWrite(PIN_GREEN_LED, HIGH);
    delay(1000);
    digitalWrite(PIN_GREEN_LED, LOW);
  } else {
    Serial.println("\nFailed to connect to Wi-Fi. Operating in Offline AP/Simulated fallback mode.");
    // Blink red LED to alert configuration issues
    for (int i = 0; i < 5; i++) {
      digitalWrite(PIN_RED_LED, HIGH);
      delay(150);
      digitalWrite(PIN_RED_LED, LOW);
      delay(150);
    }
  }
}

// --- Sensor Reading (Simulated or Physical) ---
void readSensors(float &temp, float &ph, float &doVal) {
  if (SIMULATE_SENSORS) {
    // Advance simulation time
    simTime += 0.05;

    // Simulate temperature: slow sine wave around 27.5°C +/- 1.0°C
    temp = 27.5 + (sin(simTime) * 1.0) + ((random(-10, 11) / 10.0) * 0.1);

    // Simulate pH: slow random walk around 7.1
    currentPh += (random(-5, 6) / 100.0);
    if (currentPh < 4.0) currentPh = 4.0;
    if (currentPh > 10.0) currentPh = 10.0;
    ph = currentPh;

    // Simulate Dissolved Oxygen: diurnal fluctuations around 5.8 mg/L
    // Photosynthesis increases oxygen by day, respiration drops it at night
    currentDo = 5.8 + (cos(simTime) * 1.5) + ((random(-10, 11) / 10.0) * 0.1);
    if (currentDo < 1.0) currentDo = 1.0;
    if (currentDo > 12.0) currentDo = 12.0;
    doVal = currentDo;

    Serial.printf("[SIMULATOR] Read - Temp: %.2f°C, pH: %.2f, Dissolved Oxygen: %.2f mg/L\n", temp, ph, doVal);
  } else {
    // Read physical analog values
    // Temperature: read DS18B20 or simple analog sensor
    int tempRaw = analogRead(PIN_TEMP_BUS);
    temp = (tempRaw / 4095.0) * 35.0;

    // pH sensor
    int phRaw = analogRead(PIN_PH_ANALOG);
    float phVoltage = phRaw * (3.3 / 4095.0);
    ph = 3.5 * phVoltage; 

    // Dissolved Oxygen sensor: reading analog probe output
    int doRaw = analogRead(PIN_DO_ANALOG);
    float doVoltage = doRaw * (3.3 / 4095.0);
    doVal = doVoltage * 4.5; // simple linear calibration placeholder (mg/L conversion)
    if (doVal < 0) doVal = 0;

    Serial.printf("[HARDWARE] Read - Temp: %.2f°C, pH: %.2f, DO: %.2f mg/L\n", temp, ph, doVal);
  }
}

// --- Local Threshold Indicators ---
void runLocalAlarms(float temp, float ph, float doVal) {
  bool isCritical = false;
  bool isWarning = false;

  // Temperature Alarm Checks
  if (temp < TEMP_MIN || temp > TEMP_MAX) {
    isCritical = true;
  } else if (temp < (TEMP_MIN + 1.0) || temp > (TEMP_MAX - 1.0)) {
    isWarning = true;
  }

  // pH Alarm Checks
  if (ph < PH_MIN || ph > PH_MAX) {
    isCritical = true;
  } else if (ph < (PH_MIN + 0.3) || ph > (PH_MAX - 0.3)) {
    isWarning = true;
  }

  // Dissolved Oxygen Alarm Checks
  if (doVal < DO_MIN) {
    isCritical = true;
  } else if (doVal < (DO_MIN + 1.0)) {
    isWarning = true;
  }

  // Activate hardware alerts
  if (isCritical) {
    digitalWrite(PIN_GREEN_LED, LOW);
    digitalWrite(PIN_YELLOW_LED, LOW);
    digitalWrite(PIN_RED_LED, HIGH);
    
    // Short alert beep
    digitalWrite(PIN_BUZZER, HIGH);
    delay(100);
    digitalWrite(PIN_BUZZER, LOW);
  } 
  else if (isWarning) {
    digitalWrite(PIN_GREEN_LED, LOW);
    digitalWrite(PIN_YELLOW_LED, HIGH);
    digitalWrite(PIN_RED_LED, LOW);
    digitalWrite(PIN_BUZZER, LOW);
  } 
  else {
    digitalWrite(PIN_GREEN_LED, HIGH);
    digitalWrite(PIN_YELLOW_LED, LOW);
    digitalWrite(PIN_RED_LED, LOW);
    digitalWrite(PIN_BUZZER, LOW);
  }
}

// --- HTTP JSON Upload client ---
void sendTelemetry(float temp, float ph, float doVal) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Skipping telemetry upload: Wi-Fi disconnected.");
    return;
  }

  HTTPClient http;
  char serverUrl[128];
  snprintf(serverUrl, sizeof(serverUrl), "http://%s:%d/api/telemetry", SERVER_IP, SERVER_PORT);
  
  Serial.printf("Uploading telemetry to: %s\n", serverUrl);
  http.begin(serverUrl);
  http.addHeader("Content-Type", "application/json");

  // Create JSON document
  JsonDocument doc;
  doc["temperature"] = temp;
  doc["ph"] = ph;
  doc["dissolvedOxygen"] = doVal;
  doc["isSimulated"] = (SIMULATE_SENSORS == 1);

  String requestBody;
  serializeJson(doc, requestBody);

  int httpResponseCode = http.POST(requestBody);

  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.printf("Server Response Code: %d\n", httpResponseCode);
    Serial.println("Payload: " + response);

    JsonDocument responseDoc;
    DeserializationError error = deserializeJson(responseDoc, response);
    if (!error) {
      // 1. Dynamic upload interval sync
      if (responseDoc["intervalMinutes"].is<unsigned int>()) {
        unsigned int serverInterval = responseDoc["intervalMinutes"];
        if (serverInterval != intervalMinutes && serverInterval >= 1) {
          intervalMinutes = serverInterval;
          Serial.printf("Dynamically adjusted sampling interval to: %u minutes.\n", intervalMinutes);
        }
      }

      // 2. Hardware relay controls sync
      if (responseDoc["aeratorState"].is<bool>()) {
        remoteAeratorActive = responseDoc["aeratorState"];
        Serial.printf("[SYNC] Remote Aerator Relay Status: %s\n", remoteAeratorActive ? "ON" : "OFF");
        // In a physical build, this would drive a relay module:
        // digitalWrite(PIN_GREEN_LED, remoteAeratorActive ? HIGH : LOW);
      }
      if (responseDoc["boreholePumpState"].is<bool>()) {
        remotePumpActive = responseDoc["boreholePumpState"];
        Serial.printf("[SYNC] Remote Pump Relay Status: %s\n", remotePumpActive ? "ON" : "OFF");
      }
    }
  } else {
    Serial.printf("Error uploading telemetry. HTTP Code: %s\n", http.errorToString(httpResponseCode).c_str());
  }

  http.end();
}

// --- Local Web Server Page Handlers ---
void handleRoot() {
  String html = "<html><body><h1>AquariumGuard Local ESP32 Server</h1>";
  html += "<p>Connected to Local Network. Operating status normal.</p>";
  html += "<p><a href='/status'>View Current Telemetry (/status)</a></p>";
  html += "<p><a href='/trigger_buzzer'>Trigger Buzzer Test (/trigger_buzzer)</a></p>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void handleStatus() {
  float temp, ph, doVal;
  readSensors(temp, ph, doVal);

  JsonDocument doc;
  doc["status"] = "online";
  doc["temperature"] = temp;
  doc["ph"] = ph;
  doc["dissolvedOxygen"] = doVal;
  doc["is_simulated"] = (SIMULATE_SENSORS == 1);
  doc["current_interval_mins"] = intervalMinutes;
  doc["aerator_relay"] = remoteAeratorActive ? "ON" : "OFF";
  doc["pump_relay"] = remotePumpActive ? "ON" : "OFF";

  String jsonStr;
  serializeJson(doc, jsonStr);
  server.send(200, "application/json", jsonStr);
}

void handleTriggerBuzzer() {
  Serial.println("[REMOTE COMMAND] Triggering Buzzer Alert sound test...");
  
  for (int i = 0; i < 2; i++) {
    digitalWrite(PIN_BUZZER, HIGH);
    delay(200);
    digitalWrite(PIN_BUZZER, LOW);
    delay(100);
  }

  JsonDocument doc;
  doc["status"] = "success";
  doc["message"] = "Local buzzer triggered successfully.";

  String jsonStr;
  serializeJson(doc, jsonStr);
  server.send(200, "application/json", jsonStr);
}

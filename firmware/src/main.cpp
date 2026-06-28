#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <stdarg.h>
#include "config.h"

// --- Global Variables ---
WebServer server(80);
unsigned long lastTelemetryTime = 0;
unsigned int intervalMinutes = DEFAULT_INTERVAL_MINS;

// LCD configuration
LiquidCrystal_I2C lcd(LCD_I2C_ADDRESS, 20, 4);
unsigned long lastLCDUpdateTime = 0;
const unsigned long LCD_UPDATE_INTERVAL_MS = 2000;

// DS18B20 Temperature Sensor configuration
OneWire oneWire(PIN_TEMP_BUS);
DallasTemperature tempSensor(&oneWire);

// System Status enum
enum SystemStatus {
  STATUS_OK,
  STATUS_WARNING,
  STATUS_CRITICAL
};
SystemStatus currentStatus = STATUS_OK;

// Latest sensor readings
float latestTemp = 0.0;
float latestPh = 7.1;
float latestTurbidity = 15.0;

// Simulation state
float simTime = 0.0;
float currentPh = 7.1;
float currentTurbidity = 15.0; // NTU

// Remote hardware states synced from Next.js server
bool remoteAeratorActive = true;
bool remotePumpActive = false;

// --- Function Prototypes ---
void connectToWiFi();
void readSensors(float &temp, float &ph, float &turbidity);
void runLocalAlarms(float temp, float ph, float turbidity);
void sendTelemetry(float temp, float ph, float turbidity);
void handleRoot();
void handleStatus();
void handleTriggerBuzzer();
void printLine(int row, const char* format, ...);
void updateLCD(float temp, float ph, float turbidity);

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("==========================================");
  Serial.println("AquariumGuard: Starting Smart Aquaculture...");
  Serial.println("==========================================");

  // Initialize I2C and LCD with built-in pull-ups
  pinMode(PIN_LCD_SDA, INPUT_PULLUP);
  pinMode(PIN_LCD_SCL, INPUT_PULLUP);
  Wire.begin(PIN_LCD_SDA, PIN_LCD_SCL);
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("AquariumGuard v1.0");
  lcd.setCursor(0, 1);
  lcd.print("Initializing...");

  // Initialize GPIO Pins
  pinMode(PIN_GREEN_LED, OUTPUT);
  pinMode(PIN_YELLOW_LED, OUTPUT);
  pinMode(PIN_RED_LED, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);

  // Initialize DS18B20 Sensor
  tempSensor.begin();

  // Turn off all indicators at startup
  digitalWrite(PIN_GREEN_LED, LOW);
  digitalWrite(PIN_YELLOW_LED, LOW);
  digitalWrite(PIN_RED_LED, LOW);
  digitalWrite(PIN_BUZZER, LOW);

  // Connect to the best available Local Wi-Fi
  connectToWiFi();

  // Set up local ESP32 Web Server Routes
  server.on("/", HTTP_GET, handleRoot);
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/trigger_buzzer", HTTP_GET, handleTriggerBuzzer);
  server.begin();
  Serial.println("Local HTTP Server started on port 80.");

  // Run initial sensor reading and update LCD
  readSensors(latestTemp, latestPh, latestTurbidity);
  runLocalAlarms(latestTemp, latestPh, latestTurbidity);
  updateLCD(latestTemp, latestPh, latestTurbidity);
  lastLCDUpdateTime = millis();

  // Run initial telemetry post immediately
  sendTelemetry(latestTemp, latestPh, latestTurbidity);
  lastTelemetryTime = millis();
}

void loop() {
  // Handle incoming HTTP client requests
  server.handleClient();

  // Re-attempt WiFi connection if disconnected
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WIFI] Connection lost. Re-attempting...");
    connectToWiFi();
  }

  unsigned long now = millis();

  // Real-time sensor reading, local alarms, and LCD update
  if (now - lastLCDUpdateTime >= LCD_UPDATE_INTERVAL_MS || lastLCDUpdateTime == 0) {
    readSensors(latestTemp, latestPh, latestTurbidity);
    runLocalAlarms(latestTemp, latestPh, latestTurbidity);
    updateLCD(latestTemp, latestPh, latestTurbidity);
    lastLCDUpdateTime = now;
  }

  // Check if it's time to send the next telemetry payload
  unsigned long intervalMs = TELEMETRY_INTERVAL_MS;
  if (now - lastTelemetryTime >= intervalMs || lastTelemetryTime == 0) {
    // Upload to Next.js Local Server using latest values
    sendTelemetry(latestTemp, latestPh, latestTurbidity);
    lastTelemetryTime = now;
  }

  // Brief loop yield
  delay(50);
}

// --- Multi-Network Wi-Fi Connection Manager ---
// Loops through all networks in WIFI_NETWORKS[], trying each for 10 seconds.
// Makes up to 3 full passes before giving up.
void connectToWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);
  delay(500);

  const int MAX_PASSES = 3;
  const unsigned long TIMEOUT_PER_NETWORK_MS = 10000;

  for (int pass = 0; pass < MAX_PASSES; pass++) {
    Serial.printf("[WIFI] Pass %d/%d — scanning %d configured network(s)...\n",
                  pass + 1, MAX_PASSES, WIFI_NETWORK_COUNT);

    for (int i = 0; i < WIFI_NETWORK_COUNT; i++) {
      Serial.printf("[WIFI] Trying [%d/%d]: SSID='%s'...\n",
                    i + 1, WIFI_NETWORK_COUNT, WIFI_NETWORKS[i].ssid);

      // Show on LCD
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Connecting WiFi...");
      lcd.setCursor(0, 1);
      lcd.printf("SSID: %s", WIFI_NETWORKS[i].ssid);
      lcd.setCursor(0, 2);
      lcd.printf("Pass: %d/%d", pass + 1, MAX_PASSES);

      WiFi.begin(WIFI_NETWORKS[i].ssid, WIFI_NETWORKS[i].pass);

      unsigned long startAttempt = millis();
      while (WiFi.status() != WL_CONNECTED &&
             millis() - startAttempt < TIMEOUT_PER_NETWORK_MS) {
        // Pulse yellow LED while connecting
        digitalWrite(PIN_YELLOW_LED, HIGH);
        delay(250);
        digitalWrite(PIN_YELLOW_LED, LOW);
        delay(250);
        Serial.print(".");
      }
      Serial.println();

      if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("[WIFI] Connected to '%s'!\n", WIFI_NETWORKS[i].ssid);
        Serial.print("[WIFI] ESP32 IP Address: ");
        Serial.println(WiFi.localIP());

        // Show on LCD
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("WiFi Connected!");
        lcd.setCursor(0, 1);
        lcd.print("SSID: ");
        lcd.print(WIFI_NETWORKS[i].ssid);
        lcd.setCursor(0, 2);
        lcd.print("IP Address:");
        lcd.setCursor(0, 3);
        lcd.print(WiFi.localIP().toString());
        delay(2000);

        // Flash green LED to confirm connection
        for (int j = 0; j < 2; j++) {
          digitalWrite(PIN_GREEN_LED, HIGH);
          delay(200);
          digitalWrite(PIN_GREEN_LED, LOW);
          delay(200);
        }
        digitalWrite(PIN_GREEN_LED, HIGH);
        return; // Successfully connected — exit function
      }

      Serial.printf("[WIFI] '%s' unreachable. Trying next...\n", WIFI_NETWORKS[i].ssid);
      WiFi.disconnect(true);
      delay(300);
    }
  }

  // All passes exhausted — operating in offline mode
  Serial.println("[WIFI] ERROR: Could not connect to any configured network.");
  Serial.println("[WIFI] Operating in offline/standalone mode.");

  // Show on LCD
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("WiFi Failed!");
  lcd.setCursor(0, 1);
  lcd.print("Offline Mode");
  delay(2000);

  for (int i = 0; i < 6; i++) {
    digitalWrite(PIN_RED_LED, HIGH);
    delay(150);
    digitalWrite(PIN_RED_LED, LOW);
    delay(150);
  }
}

// --- Sensor Reading (Simulated or Physical) ---
void readSensors(float &temp, float &ph, float &turbidity) {
  if (SIMULATE_SENSORS) {
    // Advance simulation time
    simTime += 0.05;

    // Simulate temperature: slow sine wave around 27.5°C ±1.0°C
    temp = 27.5 + (sin(simTime) * 1.0) + ((random(-10, 11) / 10.0) * 0.1);

    // Simulate pH: slow random walk around 7.1
    currentPh += (random(-5, 6) / 100.0);
    if (currentPh < 4.0) currentPh = 4.0;
    if (currentPh > 10.0) currentPh = 10.0;
    ph = currentPh;

    // Simulate turbidity: slow random walk in NTU, higher is worse
    currentTurbidity += (random(-20, 21) / 10.0);
    if (currentTurbidity < 0.0)   currentTurbidity = 0.0;
    if (currentTurbidity > 200.0) currentTurbidity = 200.0;
    turbidity = currentTurbidity;

    Serial.printf("[SIMULATOR] Temp: %.2f°C | pH: %.2f | Turbidity: %.1f NTU\n",
                  temp, ph, turbidity);
  } else {
    // --- Physical Sensor Readings ---

    // DS18B20 Digital Temperature Reading
    tempSensor.requestTemperatures();
    float t = tempSensor.getTempCByIndex(0);
    if (t != DEVICE_DISCONNECTED_C) {
      temp = t;
    }

    // pH sensor — linear voltage-to-pH conversion
    int phRaw = analogRead(PIN_PH_ANALOG);
    float phVoltage = phRaw * (3.3 / 4095.0);
    ph = 3.5 * phVoltage;

    // Turbidity sensor — linear voltage-to-NTU conversion
    // Higher voltage typically means cleaner water; invert to get NTU
    int turbRaw = analogRead(PIN_TURBIDITY_ANALOG);
    float turbVoltage = turbRaw * (3.3 / 4095.0);
    // Approximate: 0V = ~200 NTU, 3.3V = 0 NTU (adjust calibration as needed)
    turbidity = (1.0 - (turbVoltage / 3.3)) * 200.0;
    if (turbidity < 0.0)   turbidity = 0.0;
    if (turbidity > 200.0) turbidity = 200.0;

    Serial.printf("[HARDWARE] Temp: %.2f°C | pH: %.2f | Turbidity: %.1f NTU\n",
                  temp, ph, turbidity);
  }
}

// --- Local Threshold Indicators (LEDs + Buzzer) ---
void runLocalAlarms(float temp, float ph, float turbidity) {
  bool isCritical = false;
  bool isWarning  = false;

  // Temperature checks
  if (temp < TEMP_MIN || temp > TEMP_MAX) {
    isCritical = true;
  } else if (temp < (TEMP_MIN + 1.0) || temp > (TEMP_MAX - 1.0)) {
    isWarning = true;
  }

  // pH checks
  if (ph < PH_MIN || ph > PH_MAX) {
    isCritical = true;
  } else if (ph < (PH_MIN + 0.3) || ph > (PH_MAX - 0.3)) {
    isWarning = true;
  }

  // Turbidity checks (high NTU = bad)
  if (turbidity > TURBIDITY_MAX) {
    isCritical = true;
  } else if (turbidity > (TURBIDITY_MAX * 0.75)) {
    isWarning = true;
  }

  // Drive hardware indicators
  if (isCritical) {
    currentStatus = STATUS_CRITICAL;
    digitalWrite(PIN_GREEN_LED,  LOW);
    digitalWrite(PIN_YELLOW_LED, LOW);
    digitalWrite(PIN_RED_LED,    HIGH);
    // Buzzer: 3 long beeps for critical alert
    for (int i = 0; i < 3; i++) {
      digitalWrite(PIN_BUZZER, HIGH);
      delay(300);
      digitalWrite(PIN_BUZZER, LOW);
      if (i < 2) delay(150);
    }
  } else if (isWarning) {
    currentStatus = STATUS_WARNING;
    digitalWrite(PIN_GREEN_LED,  LOW);
    digitalWrite(PIN_YELLOW_LED, HIGH);
    digitalWrite(PIN_RED_LED,    LOW);
    // Buzzer: 2 short beeps for warning
    for (int i = 0; i < 2; i++) {
      digitalWrite(PIN_BUZZER, HIGH);
      delay(100);
      digitalWrite(PIN_BUZZER, LOW);
      if (i < 1) delay(100);
    }
  } else {
    currentStatus = STATUS_OK;
    digitalWrite(PIN_GREEN_LED,  HIGH);
    digitalWrite(PIN_YELLOW_LED, LOW);
    digitalWrite(PIN_RED_LED,    LOW);
    // Buzzer: 1 very short chirp for normal
    digitalWrite(PIN_BUZZER, HIGH);
    delay(50);
    digitalWrite(PIN_BUZZER, LOW);
  }
}

// --- HTTP JSON Upload to Next.js Server ---
void sendTelemetry(float temp, float ph, float turbidity) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[UPLOAD] Skipping: Wi-Fi disconnected.");
    return;
  }

  HTTPClient http;
  char serverUrl[128];
  snprintf(serverUrl, sizeof(serverUrl),
           "http://%s:%d/api/telemetry", SERVER_IP, SERVER_PORT);

  Serial.printf("[UPLOAD] Posting telemetry to: %s\n", serverUrl);
  http.begin(serverUrl);
  http.addHeader("Content-Type", "application/json");

  // Build JSON payload
  JsonDocument doc;
  doc["temperature"] = temp;
  doc["ph"]          = ph;
  doc["turbidity"]   = turbidity;
  doc["isSimulated"] = (SIMULATE_SENSORS == 1);

  String requestBody;
  serializeJson(doc, requestBody);

  int httpResponseCode = http.POST(requestBody);

  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.printf("[UPLOAD] Response %d: %s\n", httpResponseCode, response.c_str());

    JsonDocument responseDoc;
    DeserializationError error = deserializeJson(responseDoc, response);
    if (!error) {
      // Sync sampling interval from server
      if (responseDoc["intervalMinutes"].is<unsigned int>()) {
        unsigned int serverInterval = responseDoc["intervalMinutes"];
        if (serverInterval != intervalMinutes && serverInterval >= 1) {
          intervalMinutes = serverInterval;
          Serial.printf("[SYNC] Interval adjusted to %u min.\n", intervalMinutes);
        }
      }
      // Sync relay states
      if (responseDoc["aeratorState"].is<bool>()) {
        remoteAeratorActive = responseDoc["aeratorState"];
        Serial.printf("[SYNC] Aerator: %s\n", remoteAeratorActive ? "ON" : "OFF");
      }
      if (responseDoc["boreholePumpState"].is<bool>()) {
        remotePumpActive = responseDoc["boreholePumpState"];
        Serial.printf("[SYNC] Pump: %s\n", remotePumpActive ? "ON" : "OFF");
      }
    }
  } else {
    Serial.printf("[UPLOAD] Error: %s\n", http.errorToString(httpResponseCode).c_str());
  }

  http.end();
}

// --- Local Web Server Handlers ---
void handleRoot() {
  String html = "<html><body style='font-family:monospace;padding:20px'>";
  html += "<h2>AquariumGuard ESP32 Server</h2>";
  html += "<p>Status: <b>Online</b></p>";
  html += "<p><a href='/status'>View JSON Telemetry</a></p>";
  html += "<p><a href='/trigger_buzzer'>Test Buzzer</a></p>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void handleStatus() {
  float temp, ph, turbidity;
  readSensors(temp, ph, turbidity);

  JsonDocument doc;
  doc["status"]            = "online";
  doc["temperature"]       = temp;
  doc["ph"]                = ph;
  doc["turbidity"]         = turbidity;
  doc["is_simulated"]      = (SIMULATE_SENSORS == 1);
  doc["interval_mins"]     = intervalMinutes;
  doc["aerator_relay"]     = remoteAeratorActive ? "ON" : "OFF";
  doc["pump_relay"]        = remotePumpActive    ? "ON" : "OFF";
  doc["wifi_ssid"]         = WiFi.SSID();
  doc["esp32_ip"]          = WiFi.localIP().toString();

  String jsonStr;
  serializeJson(doc, jsonStr);
  server.send(200, "application/json", jsonStr);
}

void handleTriggerBuzzer() {
  Serial.println("[BUZZER] Remote test triggered.");
  for (int i = 0; i < 2; i++) {
    digitalWrite(PIN_BUZZER, HIGH);
    delay(200);
    digitalWrite(PIN_BUZZER, LOW);
    delay(100);
  }

  JsonDocument doc;
  doc["status"]  = "success";
  doc["message"] = "Buzzer triggered successfully.";

  String jsonStr;
  serializeJson(doc, jsonStr);
  server.send(200, "application/json", jsonStr);
}

// --- LCD Display Helpers ---

// Formats a line of text, pads/truncates to exactly 20 chars, and prints to the LCD
void printLine(int row, const char* format, ...) {
  char buf[40];
  va_list args;
  va_start(args, format);
  vsnprintf(buf, sizeof(buf), format, args);
  va_end(args);

  char line[21];
  snprintf(line, sizeof(line), "%-20.20s", buf);

  lcd.setCursor(0, row);
  lcd.print(line);
}

// Updates the parameters and statuses on the 20x4 LCD
void updateLCD(float temp, float ph, float turbidity) {
  // Row 0: Temperature
  printLine(0, "Temp: %.2f C", temp);

  // Row 1: pH Value
  printLine(1, "pH:   %.2f", ph);

  // Row 2: Turbidity NTU
  printLine(2, "Turb: %.1f NTU", turbidity);

  // Row 3: Priority action message during alerts; water clarity status when all is normal
  const char* actionMsg;
  if (turbidity > TURBIDITY_MAX) {
    actionMsg = "DO WATER EXCHANGE ";
  } else if (temp > 32.0) {
    actionMsg = "TURN ON AERATOR! ";
  } else if (temp < 25.0) {
    actionMsg = "COVER WITH TARPS ";
  } else if (ph > PH_MAX) {
    actionMsg = "FLUSH THE WATER! ";
  } else if (ph < 6.0) {
    actionMsg = "ADD DILUTE CaCO3 ";
  } else if (turbidity > (TURBIDITY_MAX * 0.75)) {
    actionMsg = "WATER MURKY-WATCH";
  } else {
    // Default: show water clarity status based on turbidity
    actionMsg = (turbidity < (TURBIDITY_MAX * 0.5)) ? "Water: CLEAN     " : "Water: DIRTY     ";
  }
  printLine(3, "%s", actionMsg);
}

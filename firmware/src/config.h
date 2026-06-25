// AquariumGuard ESP32 Configuration Header
#ifndef CONFIG_H
#define CONFIG_H

// --- Wi-Fi Credentials ---
#define WIFI_SSID "solstice 2.4GHZ
#define WIFI_PASS "71xnfzctTasP"

// --- Next.js Local Server ---
#define SERVER_IP "192.168.1.100" // Replace with your PC local IP address
#define SERVER_PORT 3000

// --- Sensor Configurations ---
#define SIMULATE_SENSORS 0 // Set to 0 to read physical pins
#define DEFAULT_INTERVAL_MINS 3

// --- Hardware GPIO Pins ---
#define PIN_GREEN_LED 18
#define PIN_YELLOW_LED 19
#define PIN_RED_LED 21
#define PIN_BUZZER 22

#define PIN_TEMP_BUS 4
#define PIN_PH_ANALOG 34
#define PIN_DO_ANALOG 35 // Analog input pin for Dissolved Oxygen sensor

// --- Sensor Local Threshold Boundaries ---
#define TEMP_MIN 26.0
#define TEMP_MAX 30.0
#define PH_MIN 6.50
#define PH_MAX 8.50
#define DO_MIN 5.0 // Safe Minimum DO in mg/L

#endif // CONFIG_H

// AquariumGuard ESP32 Configuration Header
#ifndef CONFIG_H
#define CONFIG_H

// --- Multi-Network Wi-Fi Credentials ---
// The ESP32 will scan and connect to the first available Wi-Fi network in this
// list.
struct WifiNetwork {
  const char *ssid;
  const char *pass;
};

const WifiNetwork WIFI_NETWORKS[] = {
    {"Kess", "kess1114"},           // User's Local Wi-Fi Network
    {"Kess", "hiyou122"},           // Backup/Local Dedicated IoT Network
    {"Galaxy A03s4797", "jvwb8331"} // Additional fallback network
};

const int WIFI_NETWORK_COUNT = sizeof(WIFI_NETWORKS) / sizeof(WIFI_NETWORKS[0]);

// --- Next.js Local Server ---
#define SERVER_IP "10.132.242.240" // Your PC local IP address from ipconfig
#define SERVER_PORT 3000

// --- Sensor Configurations ---
#define SIMULATE_SENSORS 0 // Set to 0 to read physical pins, 1 to simulate
#define DEFAULT_INTERVAL_MINS 3000 // Kept for server-side reference only
#define TELEMETRY_INTERVAL_MS                                                  \
  3000 // How often the ESP32 uploads to the server (milliseconds)

// --- Hardware GPIO Pins ---
#define PIN_GREEN_LED 18
#define PIN_YELLOW_LED 19
#define PIN_RED_LED 21
#define PIN_BUZZER 22

// --- Sensor Analog Pins ---
#define PIN_TEMP_BUS                                                           \
  32 // Temp sensor input pin (changed from 4 to 32 to avoid ADC2 Wi-Fi
     // conflict)
#define PIN_PH_ANALOG 34 // Analog pin for pH sensor
#define PIN_TURBIDITY_ANALOG                                                   \
  35 // Analog pin for Turbidity sensor (replacing old DO pin)

// --- Optional LCD Display (I2C) Pins ---
// Connect an I2C LiquidCrystal (e.g. 16x2 LCD with PCF8574 adapter) to these
// pins:
#define PIN_LCD_SDA 25       // SDA Pin (Data)
#define PIN_LCD_SCL 26       // SCL Pin (Clock)
#define LCD_I2C_ADDRESS 0x27 // Default I2C Address for PCF8574 LCD backpack

// --- Sensor Local Threshold Boundaries ---
#define TEMP_MIN 26.0
#define TEMP_MAX 30.0
#define PH_MIN 6.50
#define PH_MAX 8.50
#define TURBIDITY_MAX 100.0 // Safe Maximum Turbidity in NTU

#endif // CONFIG_H

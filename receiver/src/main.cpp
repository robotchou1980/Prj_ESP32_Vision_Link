#include <Arduino.h>
#include "app_controller.h"
#include "http_client_service.h"
#include "display_service.h"
#include "wifi_manager.h"

// ============================================================================
// CONFIGURATION - 請修改以下參數
// ============================================================================

// WiFi credentials - SENDER AP 資訊
const char* WIFI_SSID     = "ESP32-AP";       // SENDER 的 AP 名稱
const char* WIFI_PASSWORD = "12345678";       // SENDER 的 AP 密碼

// Camera server URL - SENDER 的 IP 與端口
const char* CAMERA_SERVER_URL = "http://192.168.4.1:80/capture";  // SENDER AP IP:PORT/capture

// ============================================================================

// Service instances
ESP32HttpClientService  httpClient;
ST7789DisplayService    displayService;
ESP32WiFiManager        wifiManager;

// Global app controller instance
AppController appController(&httpClient, &displayService, &wifiManager);

void setup() {
    Serial.begin(115200);
    delay(2000);

    Serial.println("\n\n========== ESP32 Vision Link - RECEIVER ==========");
    Serial.println("[INFO] Configured for:");
    Serial.printf("[INFO] - WiFi SSID:    %s\n", WIFI_SSID);
    Serial.printf("[INFO] - Camera URL:   %s\n", CAMERA_SERVER_URL);

    // Initialize the app controller
    appController.initialize(CAMERA_SERVER_URL, WIFI_SSID, WIFI_PASSWORD);

    Serial.println("[OK] Setup complete - starting main loop");
}

void loop() {
    // Run app controller state machine
    appController.update();

    delay(10);   // Minimal delay - tight loop for max frame rate
}


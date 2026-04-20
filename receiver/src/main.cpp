#include <Arduino.h>
#include "app_controller.h"
#include "http_client_service.h"
#include "display_service.h"

// ============================================================================
// CONFIGURATION - 请修改以下参数
// ============================================================================

// WiFi credentials - 替换为你的WiFi信息
const char* WIFI_SSID = "YOUR_SSID";           // 改为你的WiFi名称
const char* WIFI_PASSWORD = "YOUR_PASSWORD";  // 改为你的WiFi密码

// Camera server URL - 替换为你的ESP32-CAM IP地址
const char* CAMERA_SERVER_URL = "http://192.168.1.100:80";  // 改为你的ESP32-CAM IP:PORT

// ============================================================================

// Service instances
ESP32HttpClientService httpClient;
ST7789DisplayService displayService;

// Global app controller instance
AppController appController(&httpClient, &displayService);

void setup() {
    Serial.begin(115200);
    delay(2000);
    
    Serial.println("\n\n========== ESP32 Vision Link - RECEIVER ==========");
    Serial.println("[INFO] Initializing receiver application...");
    Serial.println("[INFO] Configured for:");
    Serial.print("[INFO] - WiFi SSID: ");
    Serial.println(WIFI_SSID);
    Serial.print("[INFO] - Camera URL: ");
    Serial.println(CAMERA_SERVER_URL);
    
    // Initialize the app controller with server URL and WiFi credentials
    appController.initialize(CAMERA_SERVER_URL, WIFI_SSID, WIFI_PASSWORD);
    
    Serial.println("[OK] App controller initialized and ready");
}

void loop() {
    // Run app controller state machine
    appController.update();
    
    delay(100);  // Small delay to prevent overwhelming CPU
}

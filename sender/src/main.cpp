#include <Arduino.h>
#include "camera_service.h"
#include "wifi_manager.h"
#include "http_server_service.h"

// ============================================================================
// CONFIGURATION - Edit these values for your setup
// ============================================================================
#define WIFI_SSID "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
#define SERVER_PORT 80

// ============================================================================
// Global Services (SOLID: Dependency Injection)
// ============================================================================
ESP32CameraService camera;
ESP32WiFiManager wifiManager;
ESP32HttpServerService httpServer(&camera);

// ============================================================================
// Logging and Monitoring
// ============================================================================
void printSystemStatus() {
    Serial.println("\n========== SYSTEM STATUS ==========");
    Serial.printf("WiFi Connected: %s\n", wifiManager.isConnected() ? "YES" : "NO");
    if (wifiManager.isConnected()) {
        Serial.printf("IP Address: %s\n", wifiManager.getLocalIP().c_str());
    }
    Serial.printf("HTTP Server Running: %s\n", httpServer.isRunning() ? "YES" : "NO");
    
    auto cameraStatus = camera.getStatus();
    Serial.printf("Camera Initialized: %s\n", cameraStatus.isInitialized ? "YES" : "NO");
    Serial.printf("Camera Resolution: %ux%u\n", cameraStatus.frameWidth, cameraStatus.frameHeight);
    Serial.printf("JPEG Quality: %u\n", cameraStatus.frameQuality);

    auto serverStats = httpServer.getStats();
    Serial.printf("Total Requests: %u\n", serverStats.totalRequests);
    Serial.printf("Successful Captures: %u\n", serverStats.successfulCaptures);
    Serial.printf("Failed Captures: %u\n", serverStats.failedCaptures);
    Serial.printf("Avg Response Time: %u ms\n", serverStats.averageResponseTimeMs);
    Serial.println("===================================\n");
}

// ============================================================================
// Setup Phase
// ============================================================================
void setup() {
    // Initialize Serial for debugging
    Serial.begin(115200);
    delay(1000);  // Wait for serial monitor to connect

    Serial.println("\n\n");
    Serial.println("========== ESP32-CAM SERVER STARTUP ==========");
    Serial.println("[INFO] Initializing systems...");

    // Initialize Camera
    Serial.println("[STEP] 1/3: Initializing Camera");
    if (!camera.begin()) {
        Serial.println("[FATAL] Camera initialization failed!");
        while (1) {
            delay(1000);
        }
    }
    Serial.println("[OK] Camera initialized");

    // Connect to WiFi
    Serial.println("[STEP] 2/3: Connecting to WiFi");
    if (!wifiManager.connect(WIFI_SSID, WIFI_PASSWORD, 15000)) {
        Serial.println("[FATAL] WiFi connection failed!");
        while (1) {
            delay(1000);
        }
    }
    Serial.println("[OK] WiFi connected");

    // Start HTTP Server
    Serial.println("[STEP] 3/3: Starting HTTP Server");
    if (!httpServer.begin(SERVER_PORT)) {
        Serial.println("[FATAL] HTTP server startup failed!");
        while (1) {
            delay(1000);
        }
    }
    Serial.println("[OK] HTTP server started");

    printSystemStatus();
    
    Serial.println("!!! IMPORTANT !!!");
    Serial.printf("Access camera at: http://%s:%d\n", wifiManager.getLocalIP().c_str(), SERVER_PORT);
    Serial.println("Download image: http://<IP>:%d/capture\n");
}

// ============================================================================
// Main Loop
// ============================================================================
static uint32_t lastStatusPrint = 0;
static const uint32_t STATUS_PRINT_INTERVAL = 30000;  // 30 seconds

void loop() {
    // Non-blocking HTTP request handling
    httpServer.handleClient();

    // Periodically print system status
    if (millis() - lastStatusPrint > STATUS_PRINT_INTERVAL) {
        printSystemStatus();
        lastStatusPrint = millis();
    }

    // Check WiFi connection periodically
    if (!wifiManager.isConnected()) {
        Serial.println("[WARNING] WiFi connection lost! Attempting reconnection...");
        wifiManager.connect(WIFI_SSID, WIFI_PASSWORD, 15000);
    }

    // Small delay to prevent watchdog timeout
    delay(10);
}

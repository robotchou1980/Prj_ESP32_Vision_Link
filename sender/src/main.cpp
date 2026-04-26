#include <Arduino.h>
#include "camera_service.h"
#include "http_server_service.h"
#include "wifi_manager.h"

// ============================================================================
// CONFIGURATION - Update these with your AP settings
// ============================================================================
const char* WIFI_SSID = "ESP32-AP";       // AP SSID (visible network name)
const char* WIFI_PASSWORD = "12345678";   // AP Password (min 8 characters)
const uint8_t WIFI_CHANNEL = 1;           // WiFi channel (1-13)
const uint16_t HTTP_PORT = 80;

// ============================================================================
// Global Services
// ============================================================================
ESP32WiFiManager* wifiManager = nullptr;
ESP32CameraService* cameraService = nullptr;
ESP32HttpServerService* httpServer = nullptr;

// ============================================================================
// Status Tracking
// ============================================================================
unsigned long lastStatusDisplay = 0;
const unsigned long STATUS_DISPLAY_INTERVAL = 10000;  // Display status every 10 seconds
uint32_t totalRequests = 0;

void printDeviceInfo() {
    Serial.println("\n========== DEVICE INFORMATION ==========");
    Serial.printf("ESP32 CPU Frequency: %u MHz\n", getCpuFrequencyMhz());
    Serial.printf("Flash Size: %u MB\n", ESP.getFlashChipSize() / 1024 / 1024);
    Serial.printf("Sketch Size: %u bytes\n", ESP.getSketchSize());
    Serial.println("========================================");
}

void printMemoryStats() {
    uint32_t freeHeap = esp_get_free_heap_size();
    uint32_t minFreeHeap = esp_get_minimum_free_heap_size();
    uint32_t totalHeap = ESP.getHeapSize();
    
    Serial.printf("[MEM] Heap: %u/%u bytes | Min Free: %u bytes\n", 
                  totalHeap - freeHeap, totalHeap, minFreeHeap);
}

void printSystemStatus() {
    Serial.println("\n========== SYSTEM STATUS ==========");
    
    // WiFi Status
    if (wifiManager && wifiManager->isConnected()) {
        Serial.printf("[WIFI] Connected - IP: %s\n", wifiManager->getLocalIP().c_str());
    } else {
        Serial.println("[WIFI] Disconnected");
    }
    
    // Camera Status
    if (cameraService) {
        Serial.println("[CAMERA] Initialized and ready");
    } else {
        Serial.println("[CAMERA] Not initialized");
    }
    
    // HTTP Server Status
    if (httpServer && httpServer->isRunning()) {
        Serial.printf("[HTTP] Server running on port %d\n", HTTP_PORT);
        Serial.printf("       Access at: http://%s\n", wifiManager ? wifiManager->getLocalIP().c_str() : "192.168.x.x");
    } else {
        Serial.println("[HTTP] Server not running");
    }
    
    printMemoryStats();
    Serial.println("===================================\n");
}

// ============================================================================
// Setup Phase
// ============================================================================
void setup() {
    delay(1500);  // Wait for serial port to be ready
    
    // Initialize Serial
    Serial.begin(115200);
    delay(500);
    
    // Clear initial garbage
    for (int i = 0; i < 10; i++) {
        Serial.println();
        delay(50);
    }
    
    Serial.println("\n\n");
    Serial.println("╔════════════════════════════════════════╗");
    Serial.println("║     ESP32 CAMERA WEB STREAMING         ║");
    Serial.println("║  - WiFi: ENABLED                       ║");
    Serial.println("║  - Camera: ENABLED                     ║");
    Serial.println("║  - HTTP Server: ENABLED               ║");
    Serial.println("╚════════════════════════════════════════╝");
    Serial.println();
    
    printDeviceInfo();
    
    // Initialize WiFi Manager
    Serial.println("\n[INIT] Initializing WiFi Manager...");
    wifiManager = new ESP32WiFiManager();
    
    // Start WiFi AP (Access Point mode)
    Serial.printf("[INIT] Starting WiFi AP: %s\n", WIFI_SSID);
    if (!wifiManager->startAP(WIFI_SSID, WIFI_PASSWORD, WIFI_CHANNEL)) {
        Serial.println("[ERROR] Failed to start WiFi AP!");
        Serial.println("[WARNING] Continuing without WiFi...");
    } else {
        Serial.printf("[OK] WiFi AP started! IP: %s\n", wifiManager->getLocalIP().c_str());
    }
    
    // Initialize Camera Service
    Serial.println("\n[INIT] Initializing Camera Service...");
    cameraService = new ESP32CameraService();
    if (!cameraService->begin()) {
        Serial.println("[ERROR] Failed to initialize camera!");
        delete cameraService;
        cameraService = nullptr;
    } else {
        Serial.println("[OK] Camera initialized successfully");
    }
    
    // Initialize HTTP Server
    if (cameraService) {
        delay(100);  // Give WiFi/AP time to stabilize
        Serial.println("\n[INIT] Initializing HTTP Server...");
        httpServer = new ESP32HttpServerService(cameraService);
        if (!httpServer->begin(HTTP_PORT)) {
            Serial.println("[ERROR] Failed to start HTTP server!");
            delete httpServer;
            httpServer = nullptr;
        } else {
            Serial.printf("[OK] HTTP Server started on port %d\n", HTTP_PORT);
            if (wifiManager->isConnected()) {
                Serial.printf("[INFO] Access camera at: http://%s/\n", wifiManager->getLocalIP().c_str());
            } else {
                Serial.println("[WARNING] WiFi not ready yet, HTTP server waiting...");
            }
        }
    } else {
        Serial.println("[WARNING] Skipping HTTP Server - Camera not available");
    }
    
    Serial.println("\n[OK] Setup completed successfully\n");
    printSystemStatus();
    Serial.flush();
}

// ============================================================================
// Main Loop - Handle client requests and display status
// ============================================================================
void loop() {
    // Handle HTTP client requests if server is running
    if (httpServer && httpServer->isRunning()) {
        httpServer->handleClient();
    }
    
    // Display system status periodically
    unsigned long now = millis();
    if (now - lastStatusDisplay >= STATUS_DISPLAY_INTERVAL) {
        printSystemStatus();
        lastStatusDisplay = now;
    }
    
    // Check WiFi connection
    if (wifiManager && !wifiManager->isConnected()) {
        Serial.println("[WARNING] WiFi connection lost!");
    }
    
    delay(10);  // Small delay to prevent watchdog timeout
}

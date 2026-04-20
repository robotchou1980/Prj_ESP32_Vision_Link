#include "wifi_manager.h"
#include <WiFi.h>
#include <Arduino.h>

ESP32WiFiManager::ESP32WiFiManager()
    : connected(false), localIP("") {
    WiFi.mode(WIFI_STA);
}

ESP32WiFiManager::~ESP32WiFiManager() {
    disconnect();
}

bool ESP32WiFiManager::connect(const char* ssid, const char* password, uint32_t timeoutMs) {
    if (!ssid || !password) {
        Serial.println("[ERROR] Invalid SSID or password");
        return false;
    }

    Serial.printf("[INFO] Connecting to WiFi: %s\n", ssid);
    
    WiFi.begin(ssid, password);

    uint32_t startTime = millis();
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
        
        if (millis() - startTime > timeoutMs) {
            Serial.println("\n[ERROR] WiFi connection timeout");
            return false;
        }
    }

    Serial.println("\n[INFO] WiFi connected");
    localIP = WiFi.localIP().toString().c_str();
    connected = true;
    
    Serial.printf("[INFO] IP Address: %s\n", localIP.c_str());
    Serial.printf("[INFO] Signal Strength: %d dBm\n", WiFi.RSSI());

    return true;
}

bool ESP32WiFiManager::isConnected() const {
    return WiFi.status() == WL_CONNECTED;
}

std::string ESP32WiFiManager::getLocalIP() const {
    if (isConnected()) {
        return WiFi.localIP().toString().c_str();
    }
    return "";
}

void ESP32WiFiManager::disconnect() {
    if (connected) {
        WiFi.disconnect(true);  // true = turn off WiFi
        connected = false;
        Serial.println("[INFO] WiFi disconnected");
    }
}

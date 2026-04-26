#include "wifi_manager.h"
#include <WiFi.h>
#include <Arduino.h>

ESP32WiFiManager::ESP32WiFiManager()
    : connected(false), localIP("") {
    WiFi.mode(WIFI_AP);
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
    return connected;
}

std::string ESP32WiFiManager::getLocalIP() const {
    if (connected) {
        // Return AP IP in AP mode, STA IP in STA mode
        if (WiFi.getMode() == WIFI_AP) {
            return WiFi.softAPIP().toString().c_str();
        } else {
            return WiFi.localIP().toString().c_str();
        }
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

bool ESP32WiFiManager::startAP(const char* ssid, const char* password, uint8_t channel) {
    if (!ssid || !password) {
        Serial.println("[ERROR] Invalid SSID or password");
        return false;
    }

    if (strlen(password) < 8) {
        Serial.println("[ERROR] Password must be at least 8 characters for WPA2");
        return false;
    }

    Serial.printf("[INFO] Starting WiFi AP: %s (Channel %d)\n", ssid, channel);
    
    // Start AP with SSID, password, channel, hidden=false, max_connection=4
    bool result = WiFi.softAP(ssid, password, channel, false, 4);
    
    if (result) {
        delay(100);
        localIP = WiFi.softAPIP().toString().c_str();
        connected = true;
        Serial.println("[INFO] WiFi AP started successfully");
        Serial.printf("[INFO] AP IP Address: %s\n", localIP.c_str());
        Serial.printf("[INFO] Connected Clients: %d\n", WiFi.softAPgetStationNum());
        return true;
    } else {
        Serial.println("[ERROR] Failed to start WiFi AP");
        connected = false;
        return false;
    }
}

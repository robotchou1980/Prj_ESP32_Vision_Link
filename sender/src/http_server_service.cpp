#include "http_server_service.h"
#include <WebServer.h>
#include <WiFi.h>
#include <Arduino.h>

// Global WebServer instance for callback functions
WebServer* g_server = nullptr;
ESP32HttpServerService* g_httpService = nullptr;

// Callback wrappers (C-style) required by WebServer API
void handleCaptureCallback() {
    if (g_httpService) {
        g_httpService->handleCapture();
    }
}

void handleRootCallback() {
    if (g_httpService) {
        g_httpService->handleRoot();
    }
}

void handleStatusCallback() {
    if (g_httpService) {
        g_httpService->handleStatus();
    }
}

void handleNotFoundCallback() {
    if (g_httpService) {
        g_httpService->handleNotFound();
    }
}

// Static instance tracking
static uint32_t s_totalRequests = 0;
static uint32_t s_successfulCaptures = 0;
static uint32_t s_failedCaptures = 0;
static uint32_t s_lastResponseTime = 0;

ESP32HttpServerService::ESP32HttpServerService(ICameraService* cameraService)
    : camera(cameraService), port(80), running(false) {
    if (!camera) {
        Serial.println("[ERROR] Camera service is null");
    }
}

ESP32HttpServerService::~ESP32HttpServerService() {
    end();
}

bool ESP32HttpServerService::begin(uint16_t port) {
    if (!camera) {
        Serial.println("[ERROR] Cannot start server without camera service");
        return false;
    }

    this->port = port;
    g_server = new WebServer(port);
    g_httpService = this;

    // Register request handlers
    g_server->on("/", HTTP_GET, handleRootCallback);
    g_server->on("/capture", HTTP_GET, handleCaptureCallback);
    g_server->on("/status", HTTP_GET, handleStatusCallback);
    g_server->onNotFound(handleNotFoundCallback);

    g_server->begin();
    running = true;

    Serial.printf("[INFO] HTTP Server started on port %d\n", port);
    return true;
}

void ESP32HttpServerService::end() {
    if (running && g_server) {
        g_server->stop();
        delete g_server;
        g_server = nullptr;
        g_httpService = nullptr;
        running = false;
        Serial.println("[INFO] HTTP Server stopped");
    }
}

bool ESP32HttpServerService::isRunning() const {
    return running;
}

void ESP32HttpServerService::handleClient() {
    if (running && g_server) {
        g_server->handleClient();
    }
}

void ESP32HttpServerService::handleRoot() {
    if (!g_server) return;

    s_totalRequests++;
    
    const char* html = R"(
    <html>
    <head>
        <title>ESP32-CAM Server</title>
        <meta name="viewport" content="width=device-width, initial-scale=1">
        <style>
            body { font-family: Arial; text-align: center; margin: 20px; }
            img { max-width: 100%; margin: 20px 0; }
            a { background: #0066cc; color: white; padding: 10px 20px; 
                text-decoration: none; border-radius: 5px; display: inline-block; }
        </style>
    </head>
    <body>
        <h1>ESP32-CAM Server</h1>
        <p>Live Image Feed</p>
        <img src="/capture" style="width: 100%; max-width: 320px;">
        <br>
        <a href="/capture">Download Latest Image</a>
    </body>
    </html>
    )";

    g_server->send(200, "text/html", html);
}

void ESP32HttpServerService::handleCapture() {
    if (!g_server || !camera) return;

    s_totalRequests++;
    uint32_t startTime = millis();

    // Allocate buffer for JPEG
    uint8_t* jpegBuffer = (uint8_t*)malloc(JPEG_BUFFER_SIZE);
    if (!jpegBuffer) {
        Serial.println("[ERROR] Failed to allocate JPEG buffer");
        s_failedCaptures++;
        g_server->send(500, "text/plain", "Memory allocation failed");
        return;
    }

    // Capture image
    size_t jpegSize = camera->captureJpeg(jpegBuffer, JPEG_BUFFER_SIZE);
    
    if (jpegSize == 0) {
        Serial.println("[ERROR] Failed to capture image");
        s_failedCaptures++;
        g_server->send(500, "text/plain", "Image capture failed");
        free(jpegBuffer);
        return;
    }

    // Send JPEG using proper binary response
    g_server->setContentLength(jpegSize);
    g_server->send(200, "image/jpeg");
    g_server->client().write((const uint8_t *)jpegBuffer, jpegSize);

    s_successfulCaptures++;
    s_lastResponseTime = millis() - startTime;

    Serial.printf("[INFO] Sent JPEG: %u bytes in %u ms\n", jpegSize, s_lastResponseTime);

    free(jpegBuffer);
}

void ESP32HttpServerService::handleNotFound() {
    if (!g_server) return;

    s_totalRequests++;
    g_server->send(404, "text/plain", "Not Found");
}

void ESP32HttpServerService::handleStatus() {
    if (!g_server) return;

    s_totalRequests++;
    
    // Build JSON status response
    String json = "{";
    json += "\"status\":\"ok\",";
    json += "\"uptime_ms\":" + String(millis()) + ",";
    json += "\"total_requests\":" + String(s_totalRequests) + ",";
    json += "\"successful_captures\":" + String(s_successfulCaptures) + ",";
    json += "\"failed_captures\":" + String(s_failedCaptures) + ",";
    json += "\"last_response_time_ms\":" + String(s_lastResponseTime) + ",";
    
    // Memory info - FIXED: prevent integer overflow
    uint32_t freeHeap = esp_get_free_heap_size();
    uint32_t totalHeap = ESP.getHeapSize();
    uint32_t usedHeap = (totalHeap > freeHeap) ? (totalHeap - freeHeap) : 0;
    json += "\"heap_free\":" + String(freeHeap) + ",";
    json += "\"heap_used\":" + String(usedHeap) + ",";
    json += "\"heap_total\":" + String(totalHeap) + ",";
    
    // WiFi info
    json += "\"rssi\":" + String(WiFi.RSSI());
    json += "}";
    
    g_server->send(200, "application/json", json);
}

ESP32HttpServerService::ServerStats ESP32HttpServerService::getStats() const {
    return {
        s_totalRequests,
        s_successfulCaptures,
        s_failedCaptures,
        s_lastResponseTime
    };
}

#include "http_server_service.h"
#include <WebServer.h>
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

    // Allocate buffer for JPEG (avoid stack allocation)
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

    // Send JPEG response
    g_server->sendHeader("Content-Type", "image/jpeg");
    g_server->sendHeader("Content-Length", String(jpegSize));
    g_server->send(200, "image/jpeg", "");
    
    // Send image data in chunks
    for (size_t i = 0; i < jpegSize; i += 1024) {
        size_t chunkSize = (jpegSize - i) > 1024 ? 1024 : (jpegSize - i);
        g_server->client().write(&jpegBuffer[i], chunkSize);
    }

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

ESP32HttpServerService::ServerStats ESP32HttpServerService::getStats() const {
    return {
        s_totalRequests,
        s_successfulCaptures,
        s_failedCaptures,
        s_lastResponseTime
    };
}

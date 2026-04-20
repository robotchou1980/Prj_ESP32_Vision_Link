#include "http_client_service.h"
#include <HTTPClient.h>
#include <WiFi.h>
#include <Arduino.h>
#include <string.h>

// Static tracking for statistics
static uint32_t s_totalRequests = 0;
static uint32_t s_successfulFetches = 0;
static uint32_t s_failedFetches = 0;
static uint32_t s_lastResponseTime = 0;
static uint32_t s_totalBytesReceived = 0;

ESP32HttpClientService::ESP32HttpClientService()
    : lastRequestSuccess(false), lastHttpCode(0), lastErrorMessage("") {
}

ESP32HttpClientService::~ESP32HttpClientService() {
}

bool ESP32HttpClientService::isValidJpegHeader(const uint8_t* buffer, size_t size) const {
    if (size < 2) return false;
    // JPEG SOI (Start of Image) marker: 0xFF 0xD8
    return (buffer[0] == 0xFF && buffer[1] == 0xD8);
}

bool ESP32HttpClientService::isValidJpegFooter(const uint8_t* buffer, size_t size) const {
    if (size < 2) return false;
    // JPEG EOI (End of Image) marker: 0xFF 0xD9
    return (buffer[size - 2] == 0xFF && buffer[size - 1] == 0xD9);
}

size_t ESP32HttpClientService::fetchJpeg(const char* url, uint8_t* buffer, size_t maxSize, uint32_t timeoutMs) {
    if (!url || !buffer || maxSize == 0) {
        lastErrorMessage = "Invalid parameters";
        lastRequestSuccess = false;
        return 0;
    }

    s_totalRequests++;
    uint32_t startTime = millis();
    size_t bytesReceived = 0;

    HTTPClient http;
    http.setTimeout(timeoutMs);

    Serial.printf("[INFO] Fetching from: %s\n", url);

    // Send GET request
    if (!http.begin(url)) {
        lastErrorMessage = "Failed to begin HTTP request";
        lastHttpCode = -1;
        lastRequestSuccess = false;
        s_failedFetches++;
        Serial.println("[ERROR] HTTP begin failed");
        return 0;
    }

    // Add User-Agent header
    http.addHeader("User-Agent", "ESP32-HTTPClient");

    // Send request
    int httpCode = http.GET();
    lastHttpCode = httpCode;

    // Check HTTP response code
    if (httpCode != HTTP_CODE_OK) {
        char errorMsg[64];
        snprintf(errorMsg, sizeof(errorMsg), "HTTP Error: %d", httpCode);
        lastErrorMessage = errorMsg;
        lastRequestSuccess = false;
        s_failedFetches++;
        Serial.printf("[ERROR] HTTP Status: %d\n", httpCode);
        http.end();
        return 0;
    }

    // Get content length
    int contentLength = http.getSize();
    if (contentLength <= 0 || (size_t)contentLength > maxSize) {
        char errorMsg[64];
        snprintf(errorMsg, sizeof(errorMsg), "Content size invalid: %d", contentLength);
        lastErrorMessage = errorMsg;
        lastRequestSuccess = false;
        s_failedFetches++;
        Serial.printf("[ERROR] Content length: %d\n", contentLength);
        http.end();
        return 0;
    }

    // Get WiFi stream
    WiFiClient* stream = http.getStreamPtr();
    if (!stream) {
        lastErrorMessage = "Failed to get stream";
        lastRequestSuccess = false;
        s_failedFetches++;
        http.end();
        return 0;
    }

    // Read data in chunks
    uint8_t* readPos = buffer;
    size_t remainingSize = maxSize;
    uint32_t readTimeout = timeoutMs;
    uint32_t lastReadTime = millis();

    while (stream->available() && remainingSize > 0) {
        // Check for timeout during reading
        if (millis() - lastReadTime > readTimeout) {
            lastErrorMessage = "Read timeout";
            lastRequestSuccess = false;
            s_failedFetches++;
            http.end();
            return 0;
        }

        size_t chunkSize = stream->readBytes(readPos, remainingSize > 1024 ? 1024 : remainingSize);
        if (chunkSize > 0) {
            readPos += chunkSize;
            bytesReceived += chunkSize;
            remainingSize -= chunkSize;
            lastReadTime = millis();
        } else {
            delay(1);
        }
    }

    http.end();

    // Validate JPEG format
    if (!isValidJpegHeader(buffer, bytesReceived)) {
        lastErrorMessage = "Invalid JPEG header";
        lastRequestSuccess = false;
        s_failedFetches++;
        Serial.println("[ERROR] Missing JPEG SOI marker (0xFF 0xD8)");
        return 0;
    }

    if (!isValidJpegFooter(buffer, bytesReceived)) {
        lastErrorMessage = "Invalid JPEG footer";
        lastRequestSuccess = false;
        s_failedFetches++;
        Serial.println("[ERROR] Missing JPEG EOI marker (0xFF 0xD9)");
        return 0;
    }

    // Success
    s_successfulFetches++;
    s_totalBytesReceived += bytesReceived;
    s_lastResponseTime = millis() - startTime;
    lastRequestSuccess = true;
    lastErrorMessage = "";

    Serial.printf("[INFO] Fetched JPEG: %u bytes in %u ms\n", bytesReceived, s_lastResponseTime);
    return bytesReceived;
}

bool ESP32HttpClientService::getLastRequestStatus() const {
    return lastRequestSuccess;
}

int ESP32HttpClientService::getLastHttpCode() const {
    return lastHttpCode;
}

std::string ESP32HttpClientService::getLastErrorMessage() const {
    return lastErrorMessage;
}

ESP32HttpClientService::ClientStats ESP32HttpClientService::getStats() const {
    return {
        s_totalRequests,
        s_successfulFetches,
        s_failedFetches,
        s_lastResponseTime,
        s_totalBytesReceived
    };
}

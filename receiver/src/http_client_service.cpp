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

    http.setConnectTimeout(2000);  // 2s connect timeout (fast fail if unreachable)
    http.setTimeout(timeoutMs);    // Read timeout

    Serial.printf("[INFO] Fetching from: %s\n", url);

    // Send GET request
    uint32_t t_begin = millis();
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
    uint32_t t_get = millis();
    int httpCode = http.GET();
    lastHttpCode = httpCode;
    Serial.printf("[TIMING] connect=%ums, server_response=%ums\n", t_get - t_begin, millis() - t_get);

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

    // Get content length (-1 means chunked transfer encoding)
    int contentLength = http.getSize();
    bool isChunked = (contentLength < 0);
    Serial.printf("[DEBUG] contentLength=%d, isChunked=%d\n", contentLength, (int)isChunked);
    if (!isChunked && (size_t)contentLength > maxSize) {
        char errorMsg[64];
        snprintf(errorMsg, sizeof(errorMsg), "Content too large: %d", contentLength);
        lastErrorMessage = errorMsg;
        lastRequestSuccess = false;
        s_failedFetches++;
        Serial.printf("[ERROR] Content length: %d\n", contentLength);
        http.end();
        return 0;
    }
    size_t targetSize = isChunked ? 0 : (size_t)contentLength;

    // Get WiFi stream
    WiFiClient* stream = http.getStreamPtr();
    if (!stream) {
        lastErrorMessage = "Failed to get stream";
        lastRequestSuccess = false;
        s_failedFetches++;
        http.end();
        return 0;
    }

    // Read data - handle both fixed-length and chunked transfer
    uint8_t* readPos = buffer;
    size_t remainingSize = maxSize;
    uint32_t startReadTime = millis();
    uint32_t lastDataTime  = 0;             // 0 = no data received yet
    const uint32_t CHUNKED_EOF_MS = 300;    // 300ms silence after data = EOF

    while (remainingSize > 0) {
        if (millis() - startReadTime > timeoutMs) {
            if (bytesReceived == 0) {
                lastErrorMessage = "Read timeout (no data)";
                lastRequestSuccess = false;
                s_failedFetches++;
                http.end();
                return 0;
            }
            break;  // Timeout but we have data - treat as EOF
        }

        if (stream->available() > 0) {
            size_t avail = (size_t)stream->available();
            size_t toRead = avail < remainingSize ? avail : remainingSize;  // never read more than available → no blocking
            size_t chunkSize = stream->readBytes(readPos, toRead);
            if (chunkSize > 0) {
                readPos += chunkSize;
                bytesReceived += chunkSize;
                remainingSize -= chunkSize;
                lastDataTime = millis();
                // Fixed-length: stop when all expected bytes received
                if (!isChunked && bytesReceived >= targetSize) break;
            }
        } else if (!http.connected()) {
            // Connection closed = EOF
            break;
        } else if (lastDataTime > 0 && (millis() - lastDataTime) >= CHUNKED_EOF_MS) {
            // Chunked: got data, then 300ms silence = end of body
            break;
        } else {
            yield(); // Avoid busy-wait while waiting for more data
        }
    }

    // Force-close TCP then end (prevents keep-alive drain loop)
    stream->stop();
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

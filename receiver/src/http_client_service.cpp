#include "http_client_service.h"
#include <HTTPClient.h>
#include <WiFi.h>
#include <Arduino.h>
#include <string.h>
#include <stdlib.h>

// Static tracking for statistics
static uint32_t s_totalRequests = 0;
static uint32_t s_successfulFetches = 0;
static uint32_t s_failedFetches = 0;
static uint32_t s_lastResponseTime = 0;
static uint32_t s_totalBytesReceived = 0;

ESP32HttpClientService::ESP32HttpClientService()
    : lastRequestSuccess(false), lastHttpCode(0), lastErrorMessage("")
#ifdef ENABLE_STREAM_MODE
      , streamActive(false), activeStreamUrl("")
#endif
{
}

ESP32HttpClientService::~ESP32HttpClientService() {
#ifdef ENABLE_STREAM_MODE
    closeStream();
#else
    http.end();
#endif
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

#ifdef ENABLE_STREAM_MODE
bool ESP32HttpClientService::beginStream(const char* url, uint32_t timeoutMs) {
    if (streamActive && activeStreamUrl == url && http.connected()) {
        return true;
    }

    closeStream();

    http.setConnectTimeout(2000);
    http.setTimeout(timeoutMs);

    Serial.printf("[STREAM] Connecting to: %s\n", url);

    if (!http.begin(url)) {
        lastErrorMessage = "Failed to begin stream request";
        lastHttpCode = -1;
        lastRequestSuccess = false;
        return false;
    }

    http.addHeader("User-Agent", "ESP32-StreamClient");

    int httpCode = http.GET();
    lastHttpCode = httpCode;

    if (httpCode != HTTP_CODE_OK) {
        char errorMsg[64];
        snprintf(errorMsg, sizeof(errorMsg), "Stream HTTP Error: %d", httpCode);
        lastErrorMessage = errorMsg;
        lastRequestSuccess = false;
        http.end();
        return false;
    }

    streamActive = true;
    activeStreamUrl = url;
    Serial.println("[STREAM] Connected");
    return true;
}

void ESP32HttpClientService::closeStream() {
    if (streamActive) {
        WiFiClient* stream = http.getStreamPtr();
        if (stream) {
            stream->stop();
        }
    }

    http.end();
    streamActive = false;
    activeStreamUrl = "";
}

bool ESP32HttpClientService::readLine(WiFiClient* stream, char* line, size_t maxLen, uint32_t timeoutMs) {
    if (!stream || !line || maxLen == 0) return false;

    size_t pos = 0;
    uint32_t startTime = millis();

    while ((millis() - startTime) < timeoutMs) {
        if (stream->available() > 0) {
            int c = stream->read();
            if (c < 0) {
                yield();
                continue;
            }

            if (c == '\n') {
                line[pos] = '\0';
                return true;
            }

            if (c != '\r' && pos < maxLen - 1) {
                line[pos++] = (char)c;
            }
        } else if (!http.connected()) {
            line[pos] = '\0';
            return pos > 0;
        } else {
            yield();
        }
    }

    line[pos] = '\0';
    return false;
}

bool ESP32HttpClientService::readExact(WiFiClient* stream, uint8_t* buffer, size_t length, uint32_t timeoutMs) {
    if (!stream || !buffer) return false;

    size_t bytesRead = 0;
    uint32_t startTime = millis();

    while (bytesRead < length && (millis() - startTime) < timeoutMs) {
        if (stream->available() > 0) {
            size_t avail = (size_t)stream->available();
            size_t remaining = length - bytesRead;
            size_t toRead = avail < remaining ? avail : remaining;
            size_t chunkSize = stream->readBytes(buffer + bytesRead, toRead);

            if (chunkSize > 0) {
                bytesRead += chunkSize;
                startTime = millis();
            }
        } else if (!http.connected()) {
            break;
        } else {
            yield();
        }
    }

    return bytesRead == length;
}

bool ESP32HttpClientService::readStreamFrame(WiFiClient* stream, uint8_t* buffer, size_t maxSize, size_t& frameSize, uint32_t timeoutMs) {
    frameSize = 0;

    char line[128];
    bool boundaryFound = false;
    uint32_t startTime = millis();

    while ((millis() - startTime) < timeoutMs) {
        if (!readLine(stream, line, sizeof(line), timeoutMs)) {
            break;
        }

        if (line[0] == '\0') {
            continue;
        }

        if (strncmp(line, "--frame", 7) == 0) {
            boundaryFound = true;
            break;
        }
    }

    if (!boundaryFound) {
        lastErrorMessage = "Stream boundary not found";
        lastRequestSuccess = false;
        return false;
    }

    size_t contentLength = 0;
    bool headersComplete = false;
    startTime = millis();

    while ((millis() - startTime) < timeoutMs) {
        if (!readLine(stream, line, sizeof(line), timeoutMs)) {
            break;
        }

        if (line[0] == '\0') {
            headersComplete = true;
            break;
        }

        if (strncmp(line, "Content-Length:", 15) == 0) {
            contentLength = (size_t)atoi(line + 15);
        }
    }

    if (!headersComplete || contentLength == 0) {
        lastErrorMessage = "Invalid stream frame headers";
        lastRequestSuccess = false;
        return false;
    }

    if (contentLength > maxSize) {
        char errorMsg[64];
        snprintf(errorMsg, sizeof(errorMsg), "Stream frame too large: %u", (unsigned int)contentLength);
        lastErrorMessage = errorMsg;
        lastRequestSuccess = false;
        return false;
    }

    if (!readExact(stream, buffer, contentLength, timeoutMs)) {
        lastErrorMessage = "Stream frame read timeout";
        lastRequestSuccess = false;
        return false;
    }

    if (!isValidJpegHeader(buffer, contentLength)) {
        lastErrorMessage = "Invalid stream JPEG header";
        lastRequestSuccess = false;
        return false;
    }

    if (!isValidJpegFooter(buffer, contentLength)) {
        lastErrorMessage = "Invalid stream JPEG footer";
        lastRequestSuccess = false;
        return false;
    }

    frameSize = contentLength;
    return true;
}
#endif

size_t ESP32HttpClientService::fetchJpeg(const char* url, uint8_t* buffer, size_t maxSize, uint32_t timeoutMs) {
    if (!url || !buffer || maxSize == 0) {
        lastErrorMessage = "Invalid parameters";
        lastRequestSuccess = false;
        return 0;
    }

#ifdef ENABLE_STREAM_MODE
    closeStream();
#endif

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

#ifdef ENABLE_STREAM_MODE
size_t ESP32HttpClientService::fetchStreamJpegFrame(const char* url, uint8_t* buffer, size_t maxSize, uint32_t timeoutMs) {
    if (!url || !buffer || maxSize == 0) {
        lastErrorMessage = "Invalid parameters";
        lastRequestSuccess = false;
        return 0;
    }

    s_totalRequests++;
    uint32_t startTime = millis();

    if (!beginStream(url, timeoutMs)) {
        s_failedFetches++;
        return 0;
    }

    WiFiClient* stream = http.getStreamPtr();
    if (!stream) {
        lastErrorMessage = "Failed to get stream";
        lastRequestSuccess = false;
        s_failedFetches++;
        closeStream();
        return 0;
    }

    const size_t BACKLOG_THRESHOLD_BYTES = 512;
    const uint8_t MAX_STALE_FRAMES_TO_DROP = 5;
    size_t contentLength = 0;
    uint8_t droppedFrames = 0;

    do {
        if (!readStreamFrame(stream, buffer, maxSize, contentLength, timeoutMs)) {
            s_failedFetches++;
            closeStream();
            return 0;
        }

        // The TFT decode/display path is slower than the camera stream. If TCP
        // already has another frame queued, overwrite this older frame and catch
        // up so the screen shows the freshest available image.
        int queuedBytes = stream->available();
        if (queuedBytes < (int)BACKLOG_THRESHOLD_BYTES || droppedFrames >= MAX_STALE_FRAMES_TO_DROP) {
            break;
        }

        droppedFrames++;
        yield();
    } while ((millis() - startTime) < timeoutMs);

    s_successfulFetches++;
    s_totalBytesReceived += contentLength;
    s_lastResponseTime = millis() - startTime;
    lastRequestSuccess = true;
    lastErrorMessage = "";

    if (droppedFrames > 0) {
        Serial.printf("[STREAM] Fetched latest frame: %u bytes in %u ms (dropped %u stale)\n",
                      (unsigned int)contentLength, s_lastResponseTime, droppedFrames);
    } else {
        Serial.printf("[STREAM] Fetched frame: %u bytes in %u ms\n", (unsigned int)contentLength, s_lastResponseTime);
    }
    return contentLength;
}
#endif

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

#ifndef HTTP_CLIENT_SERVICE_H
#define HTTP_CLIENT_SERVICE_H

#include <cstdint>
#include <cstddef>
#include <string>
#include <HTTPClient.h>

/**
 * @brief HTTP client interface for fetching images
 */
class IHttpClientService {
public:
    virtual ~IHttpClientService() = default;

    /**
     * @brief Fetch JPEG image from remote server
     * @param url Complete URL (e.g., "http://192.168.1.100:80/capture")
     * @param buffer Output buffer for JPEG data
     * @param maxSize Maximum buffer size
     * @param timeoutMs Request timeout in milliseconds
     * @return Size of fetched data, 0 if failed
     */
    virtual size_t fetchJpeg(const char* url, uint8_t* buffer, size_t maxSize, uint32_t timeoutMs = 5000) = 0;

    /**
     * @brief Check if last request was successful
     */
    virtual bool getLastRequestStatus() const = 0;

    /**
     * @brief Get HTTP response code from last request
     */
    virtual int getLastHttpCode() const = 0;

    /**
     * @brief Get error message from last request
     */
    virtual std::string getLastErrorMessage() const = 0;
};

/**
 * @brief ESP32 HTTPClient implementation
 * Handles HTTP GET requests for JPEG image retrieval
 */
class ESP32HttpClientService : public IHttpClientService {
private:
    HTTPClient http;           // Persistent client for TCP keep-alive / connection reuse
    bool lastRequestSuccess;
    int lastHttpCode;
    std::string lastErrorMessage;
    static const size_t MAX_URL_LENGTH = 256;
    static const size_t JPEG_BUFFER_SIZE = 64 * 1024;  // 64KB - safe for no-PSRAM ESP32

    /**
     * @brief Validate JPEG header
     */
    bool isValidJpegHeader(const uint8_t* buffer, size_t size) const;

    /**
     * @brief Validate JPEG footer
     */
    bool isValidJpegFooter(const uint8_t* buffer, size_t size) const;

public:
    ESP32HttpClientService();
    ~ESP32HttpClientService();

    size_t fetchJpeg(const char* url, uint8_t* buffer, size_t maxSize, uint32_t timeoutMs = 5000) override;
    bool getLastRequestStatus() const override;
    int getLastHttpCode() const override;
    std::string getLastErrorMessage() const override;

    /**
     * @brief Client statistics
     */
    struct ClientStats {
        uint32_t totalRequests;
        uint32_t successfulFetches;
        uint32_t failedFetches;
        uint32_t averageResponseTimeMs;
        uint32_t totalBytesReceived;
    };

    ClientStats getStats() const;
};

#endif  // HTTP_CLIENT_SERVICE_H

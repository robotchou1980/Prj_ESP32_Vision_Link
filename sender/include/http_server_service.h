#ifndef HTTP_SERVER_SERVICE_H
#define HTTP_SERVER_SERVICE_H

#include "camera_service.h"
#include <cstdint>

/**
 * @brief HTTP server interface for serving camera images
 */
class IHttpServerService {
public:
    virtual ~IHttpServerService() = default;

    /**
     * @brief Start HTTP server
     * @param port Server port (default 80)
     * @return true if started successfully
     */
    virtual bool begin(uint16_t port = 80) = 0;

    /**
     * @brief Stop HTTP server
     */
    virtual void end() = 0;

    /**
     * @brief Check if server is running
     */
    virtual bool isRunning() const = 0;

    /**
     * @brief Handle client requests (non-blocking)
     */
    virtual void handleClient() = 0;
};

/**
 * @brief ESP32 WebServer implementation
 * Serves JPEG images via HTTP /capture endpoint
 */
class ESP32HttpServerService : public IHttpServerService {
private:
    ICameraService* camera;
    uint16_t port;
    bool running;
    static const size_t JPEG_BUFFER_SIZE = 32 * 1024;

public:
    /**
     * @brief Handle GET /capture request
     */
    void handleCapture();

    /**
     * @brief Handle GET / (root) request
     */
    void handleRoot();

    /**
     * @brief Handle GET /status request (diagnostic)
     */
    void handleStatus();

    /**
     * @brief Handle not found routes
     */
    void handleNotFound();
    /**
     * @brief Constructor
     * @param cameraService Dependency injection for camera service
     */
    explicit ESP32HttpServerService(ICameraService* cameraService);
    ~ESP32HttpServerService();

    bool begin(uint16_t port = 80) override;
    void end() override;
    bool isRunning() const override;
    void handleClient() override;

    /**
     * @brief Server statistics
     */
    struct ServerStats {
        uint32_t totalRequests;
        uint32_t successfulCaptures;
        uint32_t failedCaptures;
        uint32_t averageResponseTimeMs;
    };

    ServerStats getStats() const;
};

#endif  // HTTP_SERVER_SERVICE_H

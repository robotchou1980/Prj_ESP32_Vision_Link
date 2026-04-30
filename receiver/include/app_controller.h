#ifndef APP_CONTROLLER_H
#define APP_CONTROLLER_H

#include "http_client_service.h"
#include "display_service.h"
#include "wifi_manager.h"
#include <cstdint>

/**
 * @brief Application state machine for image fetch/display cycle
 * Implements state-based control flow with non-blocking design
 * 
 * State transitions:
 * BOOT → WIFI_CONNECT → IDLE
 *     → FETCH_IMAGE → DECODE → DISPLAY → IDLE
 *     → ERROR → RETRY
 */
class AppController {
public:
    /**
     * @brief Application states
     */
    enum class AppState : uint8_t {
        BOOT = 0,
        WIFI_CONNECT,
        IDLE,
        FETCH_IMAGE,
        DECODE,
        DISPLAY_IMAGE,
        ERROR,
        RETRY
    };

    /**
     * @brief Constructor with dependency injection
     */
    AppController(IHttpClientService* httpClient, IDisplayService* display, IWiFiManager* wifiManager);

    /**
     * @brief Initialize controller and set up dependencies
     * @param serverUrl Camera server URL (e.g., "http://192.168.1.100:80")
     * @param ssid WiFi SSID
     * @param password WiFi password
     */
    void initialize(const char* serverUrl, const char* ssid, const char* password);

    /**
     * @brief Main state machine update (non-blocking)
     */
    void update();

    /**
     * @brief Get current application state
     */
    AppState getCurrentState() const;

    /**
     * @brief Get state name as string
     */
    const char* getStateName(AppState state) const;

    /**
     * @brief Get last error message
     */
    const char* getLastError() const;

private:
    IHttpClientService* httpClient;
    IDisplayService* display;
    IWiFiManager* wifiManager;
    AppState currentState;
    AppState nextState;
    uint32_t stateEntryTime;
    uint8_t retryCount;
    char lastError[256];
    char serverUrl[256];
    char wifiSsid[64];
    char wifiPassword[64];

    // State machine parameters
    static const uint32_t FETCH_INTERVAL = 100;        // 100ms breathing room for sender
    static const uint32_t WIFI_TIMEOUT = 15000;        // 15 seconds
    static const uint32_t HTTP_TIMEOUT = 10000;        // 10s timeout
    static const uint8_t MAX_RETRIES = 3;
    static const uint32_t RETRY_DELAY = 2000;          // 2 seconds

    // JPEG buffer
    uint8_t* jpegBuffer;
    size_t jpegSize;
    static const size_t MAX_JPEG_SIZE = 64 * 1024;    // 64KB - safe for no-PSRAM ESP32

    /**
     * @brief State handlers
     */
    void handleBoot();
    void handleWifiConnect();
    void handleIdle();
    void handleFetchImage();
    void handleDecode();
    void handleDisplay();
    void handleError();
    void handleRetry();

    /**
     * @brief Transition to new state
     */
    void transitionTo(AppState newState);

    /**
     * @brief Set error message
     */
    void setError(const char* errorMsg);
};

#endif  // APP_CONTROLLER_H

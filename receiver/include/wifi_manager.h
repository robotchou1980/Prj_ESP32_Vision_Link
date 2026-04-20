#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <string>

/**
 * @brief WiFi connection management interface
 * Follows Dependency Inversion principle for extensibility
 */
class IWiFiManager {
public:
    virtual ~IWiFiManager() = default;

    /**
     * @brief Connect to WiFi network
     * @param ssid Network SSID
     * @param password Network password
     * @param timeoutMs Connection timeout in milliseconds
     * @return true if connected successfully
     */
    virtual bool connect(const char* ssid, const char* password, uint32_t timeoutMs = 10000) = 0;

    /**
     * @brief Check if WiFi is connected
     * @return true if connected
     */
    virtual bool isConnected() const = 0;

    /**
     * @brief Get local IP address
     * @return IP address as string
     */
    virtual std::string getLocalIP() const = 0;

    /**
     * @brief Disconnect from WiFi
     */
    virtual void disconnect() = 0;
};

/**
 * @brief ESP32 WiFi implementation
 */
class ESP32WiFiManager : public IWiFiManager {
private:
    bool connected;
    std::string localIP;
    static const uint32_t RECONNECT_INTERVAL = 5000;  // ms

public:
    ESP32WiFiManager();
    ~ESP32WiFiManager();

    bool connect(const char* ssid, const char* password, uint32_t timeoutMs = 10000) override;
    bool isConnected() const override;
    std::string getLocalIP() const override;
    void disconnect() override;
};

#endif  // WIFI_MANAGER_H

#ifndef DISPLAY_SERVICE_H
#define DISPLAY_SERVICE_H

#include <cstdint>
#include <cstddef>

/**
 * @brief Abstract interface for display operations
 * Follows Single Responsibility and Dependency Inversion principles
 */
class IDisplayService {
public:
    virtual ~IDisplayService() = default;

    /**
     * @brief Initialize display hardware
     * @return true if initialization successful
     */
    virtual bool begin() = 0;

    /**
     * @brief Display JPEG image data
     * @param jpegData JPEG binary data
     * @param jpegSize Size of JPEG data in bytes
     * @return true if displayed successfully
     */
    virtual bool displayJpegImage(const uint8_t* jpegData, size_t jpegSize) = 0;

    /**
     * @brief Clear display and show splash screen
     */
    virtual void showSplashScreen(const char* message) = 0;

    /**
     * @brief Display error message
     * @param errorMsg Error message to display
     */
    virtual void showError(const char* errorMsg) = 0;

    /**
     * @brief Display status update
     * @param statusMsg Status message to display
     */
    virtual void showStatus(const char* statusMsg) = 0;

    /**
     * @brief Get display width in pixels
     */
    virtual uint16_t getWidth() const = 0;

    /**
     * @brief Get display height in pixels
     */
    virtual uint16_t getHeight() const = 0;

    /**
     * @brief Cleanup display resources
     */
    virtual void end() = 0;
};

/**
 * @brief TTGO T-Display implementation using ST7789 + TJpg_Decoder
 */
class ST7789DisplayService : public IDisplayService {
private:
    bool initialized;
    static const uint16_t DISPLAY_WIDTH = 135;
    static const uint16_t DISPLAY_HEIGHT = 240;
    uint32_t lastUpdateTime;

    /**
     * @brief Initialize TFT_eSPI
     */
    bool initTFT();

    /**
     * @brief Initialize TJpg_Decoder
     */
    bool initJpgDecoder();

    /**
     * @brief TJpg_Decoder callback for pixel output
     */
    static bool jpegDrawCallback(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap);

public:
    ST7789DisplayService();
    ~ST7789DisplayService();

    bool begin() override;
    bool displayJpegImage(const uint8_t* jpegData, size_t jpegSize) override;
    void showSplashScreen(const char* message) override;
    void showError(const char* errorMsg) override;
    void showStatus(const char* statusMsg) override;
    uint16_t getWidth() const override;
    uint16_t getHeight() const override;
    void end() override;

    /**
     * @brief Display statistics
     */
    struct DisplayStats {
        uint32_t totalImagesDisplayed;
        uint32_t failedDisplays;
        uint32_t lastUpdateTimeMs;
    };

    DisplayStats getStats() const;
};

#endif  // DISPLAY_SERVICE_H

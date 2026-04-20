#ifndef CAMERA_SERVICE_H
#define CAMERA_SERVICE_H

#include <vector>
#include <cstdint>

/**
 * @brief Abstract interface for camera operations
 * Follows Single Responsibility and Dependency Inversion principles
 */
class ICameraService {
public:
    virtual ~ICameraService() = default;

    /**
     * @brief Initialize camera hardware
     * @return true if initialization successful, false otherwise
     */
    virtual bool begin() = 0;

    /**
     * @brief Capture JPEG image
     * @param buffer Output buffer for JPEG data
     * @param maxSize Maximum buffer size
     * @return Size of captured image, 0 if failed
     */
    virtual size_t captureJpeg(uint8_t* buffer, size_t maxSize) = 0;

    /**
     * @brief Cleanup camera resources
     */
    virtual void end() = 0;
};

/**
 * @brief Concrete ESP32-CAM implementation
 */
class ESP32CameraService : public ICameraService {
private:
    static const size_t JPEG_BUFFER_SIZE = 32 * 1024;  // 32KB for JPEG
    uint8_t* jpegBuffer;
    bool initialized;

    /**
     * @brief Configure camera pins for AI Thinker board
     */
    void configPins();

    /**
     * @brief Configure camera frame settings
     */
    void configFrame();

public:
    ESP32CameraService();
    ~ESP32CameraService();

    bool begin() override;
    size_t captureJpeg(uint8_t* buffer, size_t maxSize) override;
    void end() override;

    /**
     * @brief Get camera status information
     */
    struct CameraStatus {
        bool isInitialized;
        uint16_t frameWidth;
        uint16_t frameHeight;
        uint8_t frameQuality;
    };

    CameraStatus getStatus() const;
};

#endif  // CAMERA_SERVICE_H

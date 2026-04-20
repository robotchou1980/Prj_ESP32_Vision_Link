#include "camera_service.h"
#include "esp_camera.h"
#include <Arduino.h>

// AI Thinker ESP32-CAM pin definitions
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

ESP32CameraService::ESP32CameraService()
    : jpegBuffer(nullptr), initialized(false) {
    jpegBuffer = (uint8_t*)malloc(JPEG_BUFFER_SIZE);
    if (!jpegBuffer) {
        Serial.println("[ERROR] Failed to allocate JPEG buffer");
    }
}

ESP32CameraService::~ESP32CameraService() {
    if (jpegBuffer) {
        free(jpegBuffer);
        jpegBuffer = nullptr;
    }
    end();
}

void ESP32CameraService::configPins() {
    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d7 = Y9_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_sccb_sda = SIOD_GPIO_NUM;
    config.pin_sccb_scl = SIOC_GPIO_NUM;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG;
    config.frame_size = FRAMESIZE_QVGA;
    config.jpeg_quality = 10;
    config.fb_count = 1;
    config.fb_location = CAMERA_FB_IN_PSRAM;
    config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("[ERROR] Camera init failed with error 0x%x\n", err);
        return;
    }

    sensor_t * s = esp_camera_sensor_get();
    s->set_brightness(s, 0);
    s->set_contrast(s, 0);
    s->set_saturation(s, 0);
    s->set_special_effect(s, 0);
    s->set_whitebal(s, 1);
    s->set_awb_gain(s, 1);
    s->set_wb_mode(s, 0);
    s->set_expose_ctrl(s, 1);
    s->set_aec2(s, 0);
    s->set_ae_level(s, 0);
    s->set_aec_value(s, 300);
    s->set_gain_ctrl(s, 1);
    s->set_agc_gain(s, 0);
    s->set_gainceiling(s, (gainceiling_t)0);
    s->set_bpc(s, 0);
    s->set_wpc(s, 1);
    s->set_raw_gma(s, 1);
    s->set_lenc(s, 1);
    s->set_hmirror(s, 0);
    s->set_vflip(s, 0);
    s->set_dcw(s, 1);
    s->set_colorbar(s, 0);
}

void ESP32CameraService::configFrame() {
    // Camera frame configuration is done in configPins()
}

bool ESP32CameraService::begin() {
    if (initialized) {
        return true;
    }

    if (!jpegBuffer) {
        Serial.println("[ERROR] JPEG buffer not allocated");
        return false;
    }

    Serial.println("[INFO] Initializing camera...");
    configPins();

    initialized = true;
    Serial.println("[INFO] Camera initialized successfully");
    return true;
}

size_t ESP32CameraService::captureJpeg(uint8_t* buffer, size_t maxSize) {
    if (!initialized || !buffer) {
        Serial.println("[ERROR] Camera not initialized or buffer invalid");
        return 0;
    }

    camera_fb_t * fb = esp_camera_fb_get();
    if (!fb) {
        Serial.println("[ERROR] Failed to get frame buffer");
        return 0;
    }

    size_t captureSize = fb->len;
    if (captureSize > maxSize) {
        Serial.printf("[WARNING] Frame size %d exceeds buffer %d, truncating\n", captureSize, maxSize);
        captureSize = maxSize;
    }

    memcpy(buffer, fb->buf, captureSize);
    esp_camera_fb_return(fb);

    return captureSize;
}

void ESP32CameraService::end() {
    if (initialized) {
        esp_camera_deinit();
        initialized = false;
        Serial.println("[INFO] Camera deinitialized");
    }
}

ESP32CameraService::CameraStatus ESP32CameraService::getStatus() const {
    return {
        initialized,
        320,   // QVGA width
        240,   // QVGA height
        10     // JPEG quality
    };
}

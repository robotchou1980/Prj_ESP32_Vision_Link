#include "display_service.h"
#include <TFT_eSPI.h>
#include <TJpg_Decoder.h>
#include <Arduino.h>

// Global TFT instance for callback
static TFT_eSPI tft;
static bool tftInitialized = false;

// Display statistics for callback
static uint32_t g_totalImagesDisplayed = 0;
static uint32_t g_failedDisplays = 0;
static uint32_t g_lastUpdateTime = 0;

/**
 * @brief TJpg_Decoder pixel callback
 * This callback is invoked by the JPEG decoder to output decoded pixels
 */
bool ST7789DisplayService::jpegDrawCallback(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
    if (!tftInitialized) return false;
    
    // Push pixels to display
    tft.pushImage(x, y, w, h, bitmap);
    return true;
}

ST7789DisplayService::ST7789DisplayService()
    : initialized(false), lastUpdateTime(0) {
}

ST7789DisplayService::~ST7789DisplayService() {
    end();
}

bool ST7789DisplayService::initTFT() {
    try {
        Serial.println("[DEBUG] Initializing TFT display...");

        // Explicitly turn ON backlight BEFORE tft.begin()
        // GPIO4 = TFT_BL on TTGO T-Display
        pinMode(TFT_BL, OUTPUT);
        digitalWrite(TFT_BL, HIGH);
        Serial.println("[DEBUG] Backlight ON (GPIO4)");
        delay(100);

        // Use proper initialization sequence with Setup25_TTGO_T_Display
        tft.begin();
        Serial.println("[DEBUG] TFT begin() completed");

        delay(100);  // Small delay after init
        tft.setRotation(0);  // Portrait mode
        Serial.println("[DEBUG] Rotation set to 0 (portrait)");
        
        // Enable byte swap - important for proper color display
        tft.setSwapBytes(true);
        Serial.println("[DEBUG] Swap bytes enabled");
        
        tft.fillScreen(TFT_BLACK);
        Serial.println("[DEBUG] Screen filled with black");
        
        tftInitialized = true;
        Serial.println("[INFO] TFT_eSPI initialized successfully");
        return true;
    } catch (...) {
        Serial.println("[ERROR] TFT_eSPI initialization failed");
        return false;
    }
}

bool ST7789DisplayService::initJpgDecoder() {
    try {
        // Set the JPEG decode callback
        TJpgDec.setCallback(jpegDrawCallback);
        
        Serial.println("[INFO] TJpg_Decoder initialized successfully");
        return true;
    } catch (...) {
        Serial.println("[ERROR] TJpg_Decoder setup failed");
        return false;
    }
}

bool ST7789DisplayService::begin() {
    if (initialized) {
        return true;
    }

    Serial.println("[INFO] Initializing display service...");

    // Initialize TFT display
    if (!initTFT()) {
        return false;
    }

    // Initialize JPEG decoder
    if (!initJpgDecoder()) {
        return false;
    }

    initialized = true;
    showSplashScreen("Initializing...");
    
    return true;
}

bool ST7789DisplayService::displayJpegImage(const uint8_t* jpegData, size_t jpegSize) {
    if (!initialized || !jpegData || jpegSize == 0) {
        Serial.println("[ERROR] Display not initialized or invalid JPEG data");
        g_failedDisplays++;
        return false;
    }

    uint32_t startTime = millis();

    // Draw JPEG directly from memory buffer
    // TJpgDec.drawJpg requires address in memory, so we need to copy to PROGMEM or use a workaround
    // For flash-based storage, we use drawJpg with proper positioning
    int16_t rc = TJpgDec.drawJpg(0, 0, (uint8_t*)jpegData, jpegSize);
    
    if (rc != 0) {
        Serial.printf("[ERROR] JPEG decode failed with code %d\n", rc);
        g_failedDisplays++;
        return false;
    }

    g_totalImagesDisplayed++;
    g_lastUpdateTime = millis() - startTime;

    Serial.printf("[INFO] Image displayed in %u ms\n", g_lastUpdateTime);
    return true;
}

void ST7789DisplayService::showSplashScreen(const char* message) {
    if (!initialized) return;

    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(2);
    tft.setTextDatum(MC_DATUM);  // Middle center
    tft.drawString(message, tft.width() / 2, tft.height() / 2);
}

void ST7789DisplayService::showError(const char* errorMsg) {
    if (!initialized) return;

    tft.fillScreen(TFT_RED);
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(1);
    tft.setTextDatum(TL_DATUM);  // Top left
    tft.drawString("ERROR:", 5, 5);
    tft.setTextSize(1);
    tft.drawString(errorMsg, 5, 25, 1);
    
    Serial.printf("[ERROR] Display: %s\n", errorMsg);
}

void ST7789DisplayService::showStatus(const char* statusMsg) {
    if (!initialized) return;

    // Always clear the bottom bar to black before drawing status text
    // This prevents text appearing on top of red error screens
    tft.fillRect(0, tft.height() - 18, tft.width(), 18, TFT_BLACK);
    tft.setTextColor(TFT_YELLOW);
    tft.setTextSize(1);
    tft.setTextDatum(BL_DATUM);
    tft.drawString(statusMsg, 5, tft.height() - 4);

    Serial.printf("[STATUS] Display: %s\n", statusMsg);
}

uint16_t ST7789DisplayService::getWidth() const {
    return DISPLAY_WIDTH;
}

uint16_t ST7789DisplayService::getHeight() const {
    return DISPLAY_HEIGHT;
}

void ST7789DisplayService::end() {
    if (initialized) {
        tft.fillScreen(TFT_BLACK);
        tftInitialized = false;
        initialized = false;
        Serial.println("[INFO] Display service stopped");
    }
}

ST7789DisplayService::DisplayStats ST7789DisplayService::getStats() const {
    return {
        g_totalImagesDisplayed,
        g_failedDisplays,
        g_lastUpdateTime
    };
}

#include "app_controller.h"
#include "wifi_manager.h"
#include <Arduino.h>
#include <string.h>

AppController::AppController(IHttpClientService* httpClient, IDisplayService* display, IWiFiManager* wifiManager)
    : httpClient(httpClient), display(display), wifiManager(wifiManager), currentState(AppState::BOOT),
      nextState(AppState::BOOT), stateEntryTime(0), retryCount(0),
      jpegSize(0) {
    
    memset(lastError, 0, sizeof(lastError));
    memset(serverUrl, 0, sizeof(serverUrl));
    memset(wifiSsid, 0, sizeof(wifiSsid));
    memset(wifiPassword, 0, sizeof(wifiPassword));

    jpegBuffer = (uint8_t*)malloc(MAX_JPEG_SIZE);
    if (!jpegBuffer) {
        Serial.println("[ERROR] Failed to allocate JPEG buffer");
    }
}

void AppController::initialize(const char* serverUrl, const char* ssid, const char* password) {
    if (!serverUrl || !ssid || !password) {
        setError("Invalid initialization parameters");
        return;
    }

    strncpy(this->serverUrl, serverUrl, sizeof(this->serverUrl) - 1);
    strncpy(this->wifiSsid, ssid, sizeof(this->wifiSsid) - 1);
    strncpy(this->wifiPassword, password, sizeof(this->wifiPassword) - 1);

    transitionTo(AppState::BOOT);
    Serial.printf("[INFO] AppController initialized with server: %s\n", serverUrl);
}

void AppController::update() {
    // State machine dispatcher
    switch (currentState) {
        case AppState::BOOT:
            handleBoot();
            break;
        case AppState::WIFI_CONNECT:
            handleWifiConnect();
            break;
        case AppState::IDLE:
            handleIdle();
            break;
        case AppState::FETCH_IMAGE:
            handleFetchImage();
            break;
        case AppState::DECODE:
            handleDecode();
            break;
        case AppState::DISPLAY_IMAGE:
            handleDisplay();
            break;
        case AppState::ERROR:
            handleError();
            break;
        case AppState::RETRY:
            handleRetry();
            break;
        default:
            transitionTo(AppState::BOOT);
            break;
    }

    // Perform transition if pending
    if (nextState != currentState) {
        currentState = nextState;
        stateEntryTime = millis();
    }
}

AppController::AppState AppController::getCurrentState() const {
    return currentState;
}

const char* AppController::getStateName(AppState state) const {
    switch (state) {
        case AppState::BOOT:         return "BOOT";
        case AppState::WIFI_CONNECT: return "WIFI_CONNECT";
        case AppState::IDLE:         return "IDLE";
        case AppState::FETCH_IMAGE:  return "FETCH_IMAGE";
        case AppState::DECODE:       return "DECODE";
        case AppState::DISPLAY_IMAGE: return "DISPLAY_IMAGE";
        case AppState::ERROR:        return "ERROR";
        case AppState::RETRY:        return "RETRY";
        default:                      return "UNKNOWN";
    }
}

const char* AppController::getLastError() const {
    return lastError;
}

void AppController::handleBoot() {
    Serial.println("[BOOT] Starting boot sequence...");

    // Initialize external devices
    if (!display || !httpClient || !wifiManager) {
        setError("Invalid dependencies");
        Serial.println("[ERROR] display, httpClient or wifiManager is null!");
        transitionTo(AppState::ERROR);
        return;
    }

    Serial.println("[BOOT] Initializing display...");
    if (!display->begin()) {
        setError("Display initialization failed");
        transitionTo(AppState::ERROR);
        return;
    }

    display->showSplashScreen("Starting...");
    transitionTo(AppState::WIFI_CONNECT);
    Serial.println("[STATE] BOOT -> WIFI_CONNECT");
}

void AppController::handleWifiConnect() {
    if (wifiManager->isConnected()) {
        Serial.printf("[STATE] WiFi already connected, IP: %s\n", wifiManager->getLocalIP().c_str());
        display->showStatus("WiFi Connected");
        transitionTo(AppState::IDLE);
        Serial.println("[STATE] WIFI_CONNECT -> IDLE");
        return;
    }

    Serial.printf("[WIFI] Connecting to: %s\n", wifiSsid);
    display->showSplashScreen("Connecting WiFi...");

    if (wifiManager->connect(wifiSsid, wifiPassword, WIFI_TIMEOUT)) {
        Serial.printf("[WIFI] Connected! IP: %s\n", wifiManager->getLocalIP().c_str());
        display->showStatus("WiFi Connected");
        transitionTo(AppState::IDLE);
        Serial.println("[STATE] WIFI_CONNECT -> IDLE");
    } else {
        setError("WiFi connection failed");
        transitionTo(AppState::ERROR);
        Serial.println("[STATE] WIFI_CONNECT -> ERROR");
    }
}

void AppController::handleIdle() {
    // Wait for fetch interval
    if (millis() - stateEntryTime >= FETCH_INTERVAL) {
        retryCount = 0;
        transitionTo(AppState::FETCH_IMAGE);
        Serial.println("[STATE] IDLE -> FETCH_IMAGE");
    }
}

void AppController::handleFetchImage() {
    jpegSize = httpClient->fetchJpeg(serverUrl, jpegBuffer, MAX_JPEG_SIZE, HTTP_TIMEOUT);

    if (jpegSize == 0) {
        display->showStatus(httpClient->getLastErrorMessage().c_str());
        setError(httpClient->getLastErrorMessage().c_str());
        transitionTo(AppState::ERROR);
        Serial.println("[STATE] FETCH_IMAGE -> ERROR");
    } else {
        // Skip DECODE state - decoding happens inside displayJpegImage
        transitionTo(AppState::DISPLAY_IMAGE);
        Serial.println("[STATE] FETCH_IMAGE -> DISPLAY_IMAGE");
    }
}

void AppController::handleDecode() {
    // Kept for state machine completeness - not used in normal flow
    transitionTo(AppState::DISPLAY_IMAGE);
}

void AppController::handleDisplay() {
    display->showStatus("Displaying...");
    
    if (!display->displayJpegImage(jpegBuffer, jpegSize)) {
        setError("Display failed");
        transitionTo(AppState::ERROR);
        Serial.println("[STATE] DISPLAY_IMAGE -> ERROR");
    } else {
        transitionTo(AppState::IDLE);
        Serial.println("[STATE] DISPLAY_IMAGE -> IDLE");
    }
}

void AppController::handleError() {
    display->showError(lastError);
    Serial.printf("[STATE] ERROR: %s\n", lastError);

    // If WiFi dropped, go back to WIFI_CONNECT
    if (!wifiManager->isConnected()) {
        Serial.println("[STATE] WiFi lost, reconnecting...");
        retryCount = 0;
        transitionTo(AppState::WIFI_CONNECT);
        return;
    }

    if (retryCount < MAX_RETRIES) {
        transitionTo(AppState::RETRY);
        Serial.println("[STATE] ERROR -> RETRY");
    } else {
        // Max retries exceeded - go back to idle but display error briefly
        delay(3000);
        retryCount = 0;
        transitionTo(AppState::IDLE);
        Serial.println("[STATE] ERROR -> IDLE (max retries exceeded)");
    }
}

void AppController::handleRetry() {
    retryCount++;
    
    // Wait before retrying
    if (millis() - stateEntryTime < RETRY_DELAY) {
        return;
    }

    transitionTo(AppState::FETCH_IMAGE);
    Serial.printf("[STATE] RETRY (%u/%u) -> FETCH_IMAGE\n", retryCount, MAX_RETRIES);
}

void AppController::transitionTo(AppState newState) {
    if (newState != currentState) {
        nextState = newState;
    }
}

void AppController::setError(const char* errorMsg) {
    if (errorMsg) {
        strncpy(lastError, errorMsg, sizeof(lastError) - 1);
        lastError[sizeof(lastError) - 1] = '\0';
    }
}

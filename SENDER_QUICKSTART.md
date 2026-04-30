# SENDER Quick Start Guide

## 5-Minute Setup

### 1. Configure WiFi Credentials

Edit `sender/src/main.cpp`:

```cpp
#define WIFI_SSID "YourNetworkName"
#define WIFI_PASSWORD "YourPassword"
#define SERVER_PORT 80
```

### 2. Build with PlatformIO

```bash
cd sender
platformio run -e esp32cam
```

### 3. Upload to ESP32-CAM

```bash
platformio run -t upload -e esp32cam
```

### 4. Monitor Serial Output

```bash
platformio device monitor
```

You should see:
```
========== ESP32-CAM SERVER STARTUP ==========
[OK] Camera initialized
[OK] WiFi connected
[OK] HTTP server started
Access camera at: http://192.168.1.XXX:80
```

### 5. Access Camera

Open browser: `http://192.168.1.XXX/capture`

---

## Running Tests Locally

```bash
# Install dependencies
cd tests
pip install -r requirements.txt

# Run all tests
pytest test_sender_*.py -v

# Expected output:
# ============================= 117 passed in 0.23s =============================
```

---

## Code Structure

```
sender/
├── src/
│   ├── main.cpp                    # Entry point, initialization
│   ├── camera_service.cpp          # Camera hardware driver
│   ├── wifi_manager.cpp            # WiFi connection management
│   └── http_server_service.cpp     # HTTP server and request handlers
│
├── include/
│   ├── camera_service.h            # Camera interface & implementation
│   ├── wifi_manager.h              # WiFi interface & implementation
│   └── http_server_service.h       # HTTP server interface & implementation
│
├── lib/                            # External libraries (if needed)
├── platformio.ini                  # Build configuration
└── doc/
    └── 04 ESP32 CAM EasyCam擴充版.pdf  # Hardware documentation
```

---

## Key Classes

### ICameraService
```cpp
class ICameraService {
    virtual bool begin() = 0;                           // Initialize
    virtual size_t captureJpeg(uint8_t* buffer, size_t maxSize) = 0;  // Capture
    virtual void end() = 0;                             // Cleanup
    virtual CameraStatus getStatus() const = 0;         // Get info
};
```

### IWiFiManager
```cpp
class IWiFiManager {
    virtual bool connect(const char* ssid, const char* password) = 0;
    virtual bool isConnected() const = 0;
    virtual std::string getLocalIP() const = 0;
    virtual void disconnect() = 0;
};
```

### IHttpServerService
```cpp
class IHttpServerService {
    virtual bool begin(uint16_t port) = 0;
    virtual void end() = 0;
    virtual bool isRunning() const = 0;
    virtual void handleClient() = 0;
};
```

---

## Testing Locally (Pure Python)

All SENDER logic can be tested locally without hardware:

### Camera Service Tests
```bash
pytest test_sender_camera.py -v
# Tests: initialization, capture, quality, resolution, buffer management
```

### HTTP Server Tests
```bash
pytest test_sender_http.py -v
# Tests: request handling, JPEG responses, error handling, statistics
```

### WiFi Manager Tests
```bash
pytest test_sender_wifi.py -v
# Tests: connection, disconnection, reconnection, signal strength
```

### Integration Tests
```bash
pytest test_sender_integration.py -v
# Tests: complete workflows, stress testing, edge cases
```

---

## Typical Workflow

1. **Write test first** (test_sender_*.py)
   ```python
   def test_camera_capture():
       camera = MockCameraService()
       assert camera.begin()
       success, data, size = camera.capture_jpeg()
       assert success and size > 0
   ```

2. **Run test** (it should fail)
   ```bash
   pytest test_sender_camera.py::test_camera_capture -v
   # FAILED - not yet implemented
   ```

3. **Implement feature**
   ```cpp
   bool ESP32CameraService::begin() {
       // ... camera init code
   }
   ```

4. **Run test again** (should pass)
   ```bash
   pytest test_sender_camera.py::test_camera_capture -v
   # PASSED
   ```

5. **Upload to device** and verify
   ```bash
   platformio run -t upload -e esp32cam
   ```

---

## Modifying Behavior

### Change Camera Resolution
Edit `sender/src/camera_service.cpp`:
```cpp
void ESP32CameraService::configPins() {
    config.frame_size = FRAMESIZE_VGA;  // Change to VGA (640×480)
}
```

### Change JPEG Quality
Edit `sender/src/camera_service.cpp`:
```cpp
void ESP32CameraService::configPins() {
    config.jpeg_quality = 20;  // Lower = smaller file (0-100)
}
```

### Add New HTTP Endpoint
Edit `sender/src/http_server_service.cpp`:
```cpp
bool ESP32HttpServerService::begin(uint16_t port) {
    // ... existing code ...
    g_server->on("/status", HTTP_GET, handleStatusCallback);
}

void ESP32HttpServerService::handleStatus() {
    g_server->send(200, "text/json", "{\"status\":\"ok\"}");
}
```

---

## Debugging Tips

### Enable Serial Logging
```cpp
Serial.begin(115200);
delay(1000);
Serial.println("[DEBUG] Starting system...");
```

### Print Camera Status
```cpp
auto status = camera.getStatus();
Serial.printf("[DEBUG] Camera: %dx%d, quality=%d\n", 
              status.frameWidth, status.frameHeight, status.frameQuality);
```

### Print Network Info
```cpp
if (wifiManager.isConnected()) {
    Serial.println(wifiManager.getLocalIP().c_str());
}
```

### Print Server Stats
```cpp
auto stats = httpServer.getStats();
Serial.printf("[DEBUG] Requests: %d, Success: %d, Failed: %d\n",
              stats.totalRequests, stats.successfulCaptures, stats.failedCaptures);
```

---

## Performance Tuning

### Reduce Memory Usage
```cpp
// In platformio.ini
build_flags =
    -DBOARD_HAS_PSRAM          # Use external PSRAM
    -Ofast                      # Fast optimization
```

### Increase Response Speed
```cpp
// In camera_service.cpp
config.jpeg_quality = 5;  # Lower quality = faster capture
config.frame_size = FRAMESIZE_QVGA;  # Smaller resolution
```

### Handle More Requests
```cpp
// In main.cpp loop
static const uint32_t SMALL_DELAY = 1;  // Reduce from 10ms
void loop() {
    httpServer.handleClient();
    delay(SMALL_DELAY);
}
```

---

## Common Issues

### Build Fails: "esp_camera.h not found"
- Check `platformio.ini` has `lib_deps = esp32-camera`
- Run `platformio lib install esp32-camera`

### Upload Fails: "Port not found"
- Check USB cable is properly connected
- Try different USB port
- Check device manager for COM port

### Camera Not Working: Black image
- Check camera ribbon cable connection
- Verify power supply (need 500mA @ 3.3V)
- Check pin configuration matches your board

### WiFi Not Connecting
- Verify SSID and password are correct
- Check WiFi frequency (2.4GHz or 5GHz)
- Move closer to router

---

## Next Steps

1. **Run local tests**: `pytest test_sender_*.py -v`
2. **Upload to device**: `platformio run -t upload -e esp32cam`
3. **Verify output**: `platformio device monitor`
4. **Access camera**: Open browser to device IP
5. **Read full guide**: See `SENDER_DEVELOPMENT.md`

---

## Support Resources

- **PlatformIO Docs**: https://docs.platformio.org
- **ESP32 Arduino**: https://github.com/espressif/arduino-esp32
- **ESP32-CAM Forum**: https://github.com/ai-thinker-community
- **Project Tests**: `tests/test_sender_*.py`

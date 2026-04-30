# ESP32 Vision Link - SENDER Development Guide

## Project Overview

The SENDER is an **ESP32-CAM** based device that captures images from a camera and serves them via HTTP to a RECEIVER device. This implementation follows **TDD** (Test-Driven Development) and **SOLID** principles with comprehensive pytest-based testing.

### Key Features

- **Hardware**: AI Thinker ESP32-CAM EasyCam Expansion Version
- **Camera Resolution**: QVGA (320×240) default, configurable up to VGA (640×480)
- **Image Format**: JPEG with configurable quality (0-100)
- **HTTP Server**: Single endpoint `/capture` returning JPEG images
- **WiFi Support**: Auto-reconnection and signal strength monitoring
- **Memory Efficient**: 32KB JPEG buffer, PSRAM support for larger images

---

## Architecture

### System Components

```
┌─────────────────────────────────────────────────────────────┐
│                  ESP32-CAM SENDER System                     │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  ┌──────────────────┐      ┌──────────────────┐            │
│  │  Camera Service  │      │  WiFi Manager    │            │
│  │ (ICameraService) │      │ (IWiFiManager)   │            │
│  └────────┬─────────┘      └────────┬─────────┘            │
│           │                         │                       │
│           └─────────────┬───────────┘                       │
│                         │                                   │
│                  ┌──────▼──────────┐                       │
│                  │  HTTP Server    │                       │
│                  │(IHttpServerSvc) │                       │
│                  └────────┬────────┘                        │
│                           │                                 │
│                     GET /capture                           │
│                           │                                 │
│                      ┌────▼────┐                           │
│                      │ JPEG    │                           │
│                      │  Data   │                           │
│                      └─────────┘                           │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### Module Responsibilities

#### 1. **CameraService** (`camera_service.h/cpp`)
- **Responsibility**: Capture JPEG images from ESP32-CAM hardware
- **Interface**: `ICameraService` (Dependency Inversion)
- **Methods**:
  - `begin()` - Initialize camera
  - `captureJpeg()` - Capture and return JPEG data
  - `end()` - Cleanup resources
  - `getStatus()` - Return camera status

#### 2. **WiFiManager** (`wifi_manager.h/cpp`)
- **Responsibility**: Manage WiFi connectivity
- **Interface**: `IWiFiManager` (Dependency Inversion)
- **Methods**:
  - `connect()` - Connect to WiFi with timeout
  - `isConnected()` - Check connection status
  - `getLocalIP()` - Get assigned IP
  - `disconnect()` - Disconnect from network

#### 3. **HttpServerService** (`http_server_service.h/cpp`)
- **Responsibility**: Serve JPEG images via HTTP
- **Interface**: `IHttpServerService` (Dependency Inversion)
- **Endpoints**:
  - `GET /` - HTML status page
  - `GET /capture` - JPEG image download
- **Methods**:
  - `begin()` - Start HTTP server
  - `end()` - Stop server
  - `handleClient()` - Non-blocking request handler
  - `getStats()` - Server statistics

---

## SOLID Principles Implementation

### 1. **Single Responsibility Principle (SRP)**

Each service has ONE reason to change:
- `CameraService` changes only if camera hardware API changes
- `WiFiManager` changes only if WiFi API changes
- `HttpServerService` changes only if HTTP protocol changes

```cpp
// ✅ Good - Single responsibility
class ICameraService {
    virtual size_t captureJpeg(uint8_t* buffer, size_t maxSize) = 0;
};

class IWiFiManager {
    virtual bool connect(const char* ssid, const char* password) = 0;
};
```

### 2. **Open/Closed Principle (OCP)**

Modules are open for extension but closed for modification:
- Use interfaces (`I*Service`) to define contracts
- New implementations can be added without modifying existing code

```cpp
// ✅ Extendable - Can add MockCameraService without changing HttpServerService
class ESP32HttpServerService : public IHttpServerService {
private:
    ICameraService* camera;  // Depends on interface, not implementation
};
```

### 3. **Liskov Substitution Principle (LSP)**

All implementations follow the same contract:
- `ESP32CameraService` and mock camera implementations are interchangeable
- Return types and semantics are consistent

```cpp
// ✅ Substitutable implementations
ICameraService* camera = new ESP32CameraService();  // Production
// Or:
ICameraService* camera = new MockCameraService();   // Testing
```

### 4. **Interface Segregation Principle (ISP)**

Clients depend only on methods they use:
- `IHttpServerService` doesn't expose internal camera details
- Each interface is focused and minimal

```cpp
// ✅ Focused interfaces
class IHttpServerService {
public:
    virtual bool begin(uint16_t port) = 0;
    virtual void handleClient() = 0;
    virtual ServerStats getStats() const = 0;
};
```

### 5. **Dependency Inversion Principle (DIP)**

High-level modules depend on abstractions, not concrete implementations:

```cpp
// ✅ Depends on interface, not implementation
class ESP32HttpServerService : public IHttpServerService {
    ESP32HttpServerService(ICameraService* camera) : camera(camera) {}
private:
    ICameraService* camera;  // Abstract interface
};
```

---

## Configuration

### main.cpp Configuration

```cpp
#define WIFI_SSID "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
#define SERVER_PORT 80
```

### Camera Settings (in camera_service.cpp)

```cpp
config.frame_size = FRAMESIZE_QVGA;    // 320x240 default
config.jpeg_quality = 10;               // 0-100, lower = smaller file
config.pixel_format = PIXFORMAT_JPEG;   // JPEG format
```

### AI Thinker ESP32-CAM Pin Configuration

The camera pins are predefined for the AI Thinker board:
```cpp
#define PWDN_GPIO_NUM     32    // Power down
#define XCLK_GPIO_NUM      0    // External clock
#define SIOD_GPIO_NUM     26    // I2C SDA
#define SIOC_GPIO_NUM     27    // I2C SCL
#define Y9_GPIO_NUM       35    // Camera data pins...
// ... (see camera_service.cpp for full pin definition)
```

---

## HTTP API Reference

### GET /capture

**Request**:
```http
GET /capture HTTP/1.1
Host: 192.168.1.100:80
```

**Response** (Success):
```http
HTTP/1.1 200 OK
Content-Type: image/jpeg
Content-Length: 8192

[Binary JPEG data]
```

**Response** (Error):
```http
HTTP/1.1 500 Internal Server Error
Content-Type: text/plain

Capture failed
```

### GET /

**Request**:
```http
GET / HTTP/1.1
Host: 192.168.1.100:80
```

**Response**:
```http
HTTP/1.1 200 OK
Content-Type: text/html

<html>
    <h1>ESP32-CAM Server</h1>
    <img src="/capture">
</html>
```

---

## Testing Strategy

### Test Files

| File | Coverage | Tests |
|------|----------|-------|
| `test_sender_camera.py` | Camera initialization, capture, quality, resolution | 32 |
| `test_sender_http.py` | HTTP requests, responses, statistics | 29 |
| `test_sender_wifi.py` | WiFi connection, disconnection, signal | 30 |
| `test_sender_integration.py` | Complete workflows, stress tests | 26 |

### Running Tests

```bash
cd tests

# Run all SENDER tests
pytest test_sender_*.py -v

# Run with coverage
pytest test_sender_*.py --cov --cov-report=html

# Run specific test
pytest test_sender_camera.py::TestJpegCapture::test_successful_capture -v
```

### Test Coverage

✅ **117 tests passing** covering:
- Initialization and shutdown
- JPEG capture and encoding
- WiFi connection/disconnection
- HTTP request handling
- Error recovery
- Statistics and monitoring
- Stress testing (50+ rapid requests)
- Edge cases and boundary conditions

---

## Building and Uploading

### Prerequisites

```bash
# Install PlatformIO
pip install platformio

# Or use VS Code PlatformIO Extension
```

### Build

```bash
cd sender
platformio run -e esp32cam
```

### Upload to Device

```bash
cd sender
platformio run -t upload -e esp32cam
```

### Monitor Serial Output

```bash
cd sender
platformio device monitor
```

---

## Runtime Flow

```
BOOT
  │
  ├─→ Initialize Serial
  │
  ├─→ Initialize Camera
  │   └─→ Configure pins
  │   └─→ Configure frame settings
  │
  ├─→ Connect to WiFi
  │   └─→ Retry with timeout
  │   └─→ Print IP address
  │
  ├─→ Start HTTP Server
  │   └─→ Register handlers
  │
LOOP
  ├─→ Handle HTTP Client requests (non-blocking)
  │   │
  │   ├─→ GET / → Serve HTML
  │   │
  │   └─→ GET /capture → Capture JPEG & Send
  │       ├─→ Allocate JPEG buffer
  │       ├─→ Capture from camera
  │       ├─→ Send JPEG data
  │       └─→ Track statistics
  │
  ├─→ Print status (every 30 seconds)
  │
  └─→ Check WiFi connection (auto-reconnect if needed)
```

---

## Performance Characteristics

### Memory Usage

| Component | Size | Notes |
|-----------|------|-------|
| JPEG Buffer | 32 KB | On heap, freed after each request |
| Code | ~50 KB | Varies with optimization |
| Stack | ~4 KB | Per task |

### Network Performance

| Metric | Value | Notes |
|--------|-------|-------|
| Max JPEG Size | 32 KB | QVGA @ quality 10 |
| Response Time | ~50 ms | For typical capture |
| Requests/sec | ~20 | On LAN with 100Mbps |

### Camera Performance

| Parameter | Value | Notes |
|-----------|-------|-------|
| Default Resolution | QVGA (320×240) | Configurable to VGA |
| Default Quality | 10 (0-100) | Lower = smaller file |
| Typical Capture Time | ~50 ms | Hardware dependent |

---

## Error Handling

### Camera Errors

```cpp
// Handle initialization failure
if (!camera.begin()) {
    Serial.println("[FATAL] Camera initialization failed!");
    while (1) delay(1000);  // Halt
}

// Handle capture failure
size_t jpegSize = camera.captureJpeg(buffer, maxSize);
if (jpegSize == 0) {
    // Retry or return error to client
    httpServer.send(500, "text/plain", "Image capture failed");
}
```

### WiFi Errors

```cpp
// Handle connection failure with timeout
if (!wifiManager.connect(WIFI_SSID, WIFI_PASSWORD, 15000)) {
    Serial.println("[FATAL] WiFi connection failed!");
    // Retry logic
}

// Monitor and auto-reconnect
if (!wifiManager.isConnected()) {
    Serial.println("[WARNING] WiFi connection lost!");
    wifiManager.connect(WIFI_SSID, WIFI_PASSWORD, 15000);
}
```

### HTTP Errors

```cpp
// Handle invalid paths
void handleNotFound() {
    server.send(404, "text/plain", "Not Found");
}

// Handle server errors
void handleCapture() {
    if (!jpegBuffer) {
        server.send(500, "text/plain", "Memory allocation failed");
        return;
    }
}
```

---

## Debugging

### Serial Output

The SENDER prints detailed logs to serial (115200 baud):

```
========== ESP32-CAM SERVER STARTUP ==========
[INFO] Initializing systems...
[STEP] 1/3: Initializing Camera
[OK] Camera initialized
[STEP] 2/3: Connecting to WiFi
[OK] WiFi connected
[STEP] 3/3: Starting HTTP Server
[OK] HTTP server started

========== SYSTEM STATUS ==========
WiFi Connected: YES
IP Address: 192.168.1.100
HTTP Server Running: YES
Camera Initialized: YES
Camera Resolution: 320x240
JPEG Quality: 10
Total Requests: 42
Successful Captures: 40
Failed Captures: 2
Avg Response Time: 52 ms
===================================
```

### Enable Debug Logging

```cpp
// In platformio.ini
build_flags =
    -DCORE_DEBUG_LEVEL=3  # Enable debug logs
```

---

## SOLID Compliance Checklist

✅ **Single Responsibility**: Each service has one reason to change
✅ **Open/Closed**: Open for extension (new services), closed for modification
✅ **Liskov Substitution**: All implementations are interchangeable
✅ **Interface Segregation**: Focused, minimal interfaces
✅ **Dependency Inversion**: Depends on abstractions, not concretions

---

## Common Issues and Solutions

### Issue: "Camera initialization failed"
- Check ESP32-CAM is powered with sufficient current (3.3V, 500mA+)
- Verify pin connections match AI Thinker configuration
- Ensure PSRAM is enabled in board settings

### Issue: "WiFi connection timeout"
- Check SSID and password are correct
- Verify WiFi signal strength
- Increase timeout value in `main.cpp`

### Issue: "HTTP server not responding"
- Check PORT 80 is available (no other service running)
- Verify device has valid IP address
- Check firewall isn't blocking port 80

### Issue: "JPEG buffer allocation failed"
- Reduce JPEG_BUFFER_SIZE if memory is constrained
- Free other resources before startup
- Enable PSRAM support

---

## Future Enhancements

- [ ] Add MJPEG streaming support
- [ ] Implement basic authentication
- [ ] Add motion detection
- [ ] Support multiple image resolutions
- [ ] Add OTA firmware updates
- [ ] Implement SSL/TLS encryption
- [ ] Add configuration via web interface

---

## References

- [ESP32-CAM Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/)
- [AI Thinker ESP32-CAM](https://github.com/ai-thinker-community)
- [Arduino Framework for ESP32](https://github.com/espressif/arduino-esp32)
- [SOLID Principles](https://en.wikipedia.org/wiki/SOLID)

---

## License

This project is part of ESP32 Vision Link system. See main LICENSE file for details.

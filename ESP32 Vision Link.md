# ESP32 Camera to TTGO Display System (TDD + SOLID + Pytest)

## 🎯 Objective

Generate a complete embedded system using two ESP32 devices:

1. ESP32-CAM (Sender)
2. TTGO T-Display ESP32 (Receiver)

System flow:

ESP32-CAM → WiFi (HTTP) → TTGO → LCD Display

The system should:
- Capture JPEG images on ESP32-CAM
- Serve via HTTP `/capture`
- TTGO fetches image every 1 second
- Decode and display on ST7789 LCD

---

## 🧠 Engineering Requirements

### Architecture Principles

Apply **SOLID principles**:

- **S**: Single Responsibility  
  - Separate Camera, WiFi, HTTP, Display logic
- **O**: Open/Closed  
  - Allow replacing transport layer (HTTP → TCP)
- **L**: Liskov Substitution  
  - Abstract interfaces for transport/display
- **I**: Interface Segregation  
  - Do not create large monolithic classes
- **D**: Dependency Inversion  
  - High-level modules depend on abstractions

---

## 🧪 Development Methodology (TDD)

Follow **Test-Driven Development (TDD)**:

1. Write test first (host-side logic)
2. Implement minimal code
3. Refactor

---

## 🧪 Pytest Requirements (Host-side Testing)

Even though ESP32 cannot run pytest, you MUST:

- Extract **testable logic into pure C/C++ modules**
- Provide Python pytest examples for:
  - Frame parsing
  - HTTP response validation
  - Buffer handling logic
  - State machine (fetch → decode → display)

Example test targets:

- Image buffer size validation
- HTTP response parsing
- Retry logic
- Timeout handling

---

## 🏗 Project Structure

Generate TWO PlatformIO projects:

/sender
├── src/main.cpp
├── lib/
├── include/
└── platformio.ini

/receiver
├── src/main.cpp
├── lib/
├── include/
└── platformio.ini

/tests
├── test_http_parser.py
├── test_buffer.py
├── test_state_machine.py


---

## 📡 Sender (ESP32-CAM)

### Requirements

- Board: AI Thinker ESP32-CAM
- Connect to WiFi
- Start HTTP server
- Endpoint: `/capture`
- Returns: `image/jpeg`

### Design

Modules:

- CameraService
- HttpServerService
- WiFiManager

### Constraints

- Use JPEG
- Resolution: QQVGA or QVGA
- Avoid memory fragmentation
- Handle capture failure

---

## 📺 Receiver (TTGO T-Display)

### Requirements

- Board: TTGO T-Display ESP32
- Display: ST7789 (via TFT_eSPI)
- Fetch image every 1000 ms
- Decode JPEG
- Display to LCD

### Design

Modules:

- WiFiManager
- HttpClientService
- ImageDecoder
- DisplayService
- AppController (state machine)

---

## 🔁 Runtime Flow (Receiver)

BOOT → WIFI_CONNECT → IDLE
→ FETCH_IMAGE → DECODE → DISPLAY → IDLE
→ ERROR → RETRY


---

## ⚠️ Embedded Constraints

- RAM is limited (~300KB usable)
- Avoid large static buffers
- Prefer streaming over full buffering
- Non-blocking design preferred (millis-based)

---

## 📦 Libraries

Sender:
- esp32-camera
- WiFi
- WebServer

Receiver:
- WiFi
- HTTPClient
- TFT_eSPI
- TJpg_Decoder

---

## 🧪 Pytest Examples (Required)

Provide Python tests for:

### 1. HTTP response parsing

- Validate header extraction
- Validate JPEG start/end markers

### 2. Buffer handling

- Chunked data reconstruction
- Boundary detection

### 3. State machine

- Retry logic
- Timeout handling

---

## 📄 Output Requirements

Generate ALL of the following:

### Sender
- main.cpp
- camera module
- HTTP server module
- platformio.ini

### Receiver
- main.cpp
- display module
- HTTP client module
- image decoder wrapper
- platformio.ini

### Tests
- pytest files for logic validation

---

## 🚫 Explicit Constraints

DO NOT:

- Use WebSocket / RTSP / UDP streaming
- Implement MJPEG streaming
- Assume large memory buffers
- Use blocking delays (>100ms)

---

## ✅ Success Criteria

System is considered successful if:

- TTGO displays image every 1 second
- System runs continuously without crash
- Basic error recovery works
- Code is modular and testable

---

## 💡 Additional Notes

- Mark any hardware-dependent section clearly
- Highlight parts needing manual config (e.g. TFT_eSPI setup)
- Add serial debug logs

---

## 🎯 Goal

This is NOT a demo-only script.

This should be:

- Maintainable
- Testable
- Extensible
- Embedded-production-oriented code

Refactor the receiver code to strictly follow SOLID.
Separate state machine, transport, and display layers.
Do not mix hardware logic with control logic.


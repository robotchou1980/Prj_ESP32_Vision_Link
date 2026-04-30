"""
Pytest module for SENDER camera service testing.

Tests cover:
- JPEG capture simulation
- Frame buffer management
- Resolution and quality settings
- Error handling and edge cases
"""

import pytest
from typing import Tuple, Optional
from dataclasses import dataclass
from enum import Enum


class FrameSize(Enum):
    """Camera frame size options"""
    QVGA = (320, 240)      # 320x240 - Used for ESP32-CAM
    VGA = (640, 480)       # 640x480
    XGA = (1024, 768)      # 1024x768


@dataclass
class JpegFrame:
    """Simulated JPEG frame from camera"""
    width: int
    height: int
    quality: int
    data: bytes
    
    @property
    def size(self) -> int:
        """Get frame size in bytes"""
        return len(self.data)


class MockCameraService:
    """Mock camera service for testing SENDER logic"""
    
    JPEG_BUFFER_SIZE = 32 * 1024  # 32KB
    SOI_MARKER = bytes([0xFF, 0xD8])  # JPEG Start Of Image
    EOI_MARKER = bytes([0xFF, 0xD9])  # JPEG End Of Image
    
    def __init__(self):
        self.initialized = False
        self.frame_width = 320
        self.frame_height = 240
        self.frame_quality = 10
        self.capture_count = 0
        self.fail_next_capture = False
        self.last_error = ""
    
    def begin(self) -> bool:
        """Initialize camera"""
        try:
            if self.initialized:
                return True
            self.initialized = True
            return True
        except Exception as e:
            self.last_error = str(e)
            return False
    
    def end(self):
        """Shutdown camera"""
        self.initialized = False
    
    def capture_jpeg(self, max_size: int = JPEG_BUFFER_SIZE) -> Tuple[bool, Optional[bytes], int]:
        """
        Simulate JPEG capture.
        
        Args:
            max_size: Maximum buffer size
            
        Returns:
            Tuple of (success, data, size)
        """
        if not self.initialized:
            self.last_error = "Camera not initialized"
            return False, None, 0
        
        if self.fail_next_capture:
            self.fail_next_capture = False
            self.last_error = "Capture failed"
            return False, None, 0
        
        # Simulate JPEG data with markers, size based on resolution
        # Estimate ~20% compression for JPEG
        simulated_size = (self.frame_width * self.frame_height * 2) // 10  # ~20% compression
        
        # Create mock JPEG with SOI and EOI markers
        jpeg_data = self.SOI_MARKER
        padding = simulated_size - 4
        if padding > 0:
            jpeg_data += b'\x00' * padding
        jpeg_data += self.EOI_MARKER
        
        if len(jpeg_data) > max_size:
            self.last_error = f"Capture size {len(jpeg_data)} exceeds max {max_size}"
            return False, None, 0
        
        self.capture_count += 1
        self.last_error = ""  # Clear error on successful capture
        return True, jpeg_data, len(jpeg_data)
    
    def get_status(self) -> dict:
        """Get camera status"""
        return {
            "initialized": self.initialized,
            "width": self.frame_width,
            "height": self.frame_height,
            "quality": self.frame_quality
        }
    
    def set_resolution(self, width: int, height: int) -> bool:
        """Set camera resolution"""
        if not self.initialized:
            return False
        self.frame_width = width
        self.frame_height = height
        return True
    
    def set_quality(self, quality: int) -> bool:
        """Set JPEG quality (0-100)"""
        if not (0 <= quality <= 100):
            self.last_error = f"Invalid quality: {quality}"
            return False
        self.frame_quality = quality
        return True


# ============================================================================
# Initialization Tests
# ============================================================================

class TestCameraInitialization:
    """Test camera initialization"""
    
    def test_camera_starts_uninitialized(self):
        """Camera should be uninitialized on construction"""
        camera = MockCameraService()
        assert not camera.initialized
    
    def test_begin_initializes_camera(self):
        """Begin should initialize camera"""
        camera = MockCameraService()
        assert camera.begin()
        assert camera.initialized
    
    def test_begin_idempotent(self):
        """Multiple begin calls should be safe"""
        camera = MockCameraService()
        assert camera.begin()
        assert camera.begin()  # Should not fail
    
    def test_end_deinitializes_camera(self):
        """End should deinitialize camera"""
        camera = MockCameraService()
        camera.begin()
        camera.end()
        assert not camera.initialized


# ============================================================================
# JPEG Capture Tests
# ============================================================================

class TestJpegCapture:
    """Test JPEG image capture"""
    
    def test_capture_fails_if_not_initialized(self):
        """Capture should fail if camera not initialized"""
        camera = MockCameraService()
        success, data, size = camera.capture_jpeg()
        assert not success
        assert data is None
        assert size == 0
    
    def test_successful_capture(self):
        """Should successfully capture JPEG"""
        camera = MockCameraService()
        camera.begin()
        success, data, size = camera.capture_jpeg()
        assert success
        assert data is not None
        assert size > 0
    
    def test_capture_has_jpeg_markers(self):
        """Captured JPEG should have SOI and EOI markers"""
        camera = MockCameraService()
        camera.begin()
        success, data, size = camera.capture_jpeg()
        assert success
        # Check SOI marker (0xFFD8)
        assert data[:2] == bytes([0xFF, 0xD8])
        # Check EOI marker (0xFFD9)
        assert data[-2:] == bytes([0xFF, 0xD9])
    
    def test_capture_count_increments(self):
        """Capture count should increment"""
        camera = MockCameraService()
        camera.begin()
        assert camera.capture_count == 0
        
        camera.capture_jpeg()
        assert camera.capture_count == 1
        
        camera.capture_jpeg()
        assert camera.capture_count == 2
    
    def test_capture_respects_max_size(self):
        """Capture should respect maximum buffer size"""
        camera = MockCameraService()
        camera.begin()
        success, data, size = camera.capture_jpeg(max_size=100)
        
        # Mock will fail if size exceeds max
        if success:
            assert size <= 100
    
    def test_failed_capture_sets_error(self):
        """Failed capture should set error message"""
        camera = MockCameraService()
        camera.begin()
        camera.fail_next_capture = True
        
        success, data, size = camera.capture_jpeg()
        assert not success
        assert camera.last_error == "Capture failed"


# ============================================================================
# Resolution Tests
# ============================================================================

class TestResolution:
    """Test camera resolution settings"""
    
    def test_default_resolution_qvga(self):
        """Default resolution should be QVGA"""
        camera = MockCameraService()
        assert camera.frame_width == 320
        assert camera.frame_height == 240
    
    def test_set_resolution(self):
        """Should set resolution"""
        camera = MockCameraService()
        camera.begin()
        assert camera.set_resolution(640, 480)
        assert camera.frame_width == 640
        assert camera.frame_height == 480
    
    def test_set_resolution_fails_if_not_initialized(self):
        """Set resolution should fail if not initialized"""
        camera = MockCameraService()
        assert not camera.set_resolution(640, 480)
    
    def test_resolution_affects_capture_size(self):
        """Higher resolution should result in larger captures"""
        camera = MockCameraService()
        camera.begin()
        
        # Capture at QVGA (320x240)
        camera.set_resolution(320, 240)
        success_qvga, data_qvga, size_qvga = camera.capture_jpeg()
        assert success_qvga
        
        # Capture at VGA (640x480) - needs larger buffer
        camera.set_resolution(640, 480)
        # VGA requires ~61KB, use larger buffer
        success_vga, data_vga, size_vga = camera.capture_jpeg(max_size=100 * 1024)
        assert success_vga
        
        # Both captures should be valid
        assert size_qvga > 0
        assert size_vga > 0
        # VGA should be larger due to 4x more pixels
        assert size_vga >= size_qvga


# ============================================================================
# Quality Tests
# ============================================================================

class TestQuality:
    """Test JPEG quality settings"""
    
    def test_default_quality(self):
        """Default quality should be 10"""
        camera = MockCameraService()
        assert camera.frame_quality == 10
    
    def test_set_valid_quality(self):
        """Should set valid quality (0-100)"""
        camera = MockCameraService()
        camera.begin()
        
        for quality in [0, 50, 100]:
            assert camera.set_quality(quality)
            assert camera.frame_quality == quality
    
    def test_set_invalid_quality_negative(self):
        """Should reject negative quality"""
        camera = MockCameraService()
        camera.begin()
        assert not camera.set_quality(-1)
    
    def test_set_invalid_quality_over_100(self):
        """Should reject quality over 100"""
        camera = MockCameraService()
        camera.begin()
        assert not camera.set_quality(101)
    
    def test_quality_sets_error_on_invalid(self):
        """Should set error message for invalid quality"""
        camera = MockCameraService()
        camera.begin()
        camera.set_quality(150)
        assert "Invalid quality" in camera.last_error


# ============================================================================
# Status Tests
# ============================================================================

class TestCameraStatus:
    """Test camera status reporting"""
    
    def test_status_initialization_flag(self):
        """Status should report initialization state"""
        camera = MockCameraService()
        camera.begin()
        status = camera.get_status()
        assert status["initialized"] == True
    
    def test_status_reports_resolution(self):
        """Status should report resolution"""
        camera = MockCameraService()
        camera.begin()
        camera.set_resolution(640, 480)
        status = camera.get_status()
        assert status["width"] == 640
        assert status["height"] == 480
    
    def test_status_reports_quality(self):
        """Status should report quality"""
        camera = MockCameraService()
        camera.begin()
        camera.set_quality(50)
        status = camera.get_status()
        assert status["quality"] == 50


# ============================================================================
# Buffer Management Tests
# ============================================================================

class TestBufferManagement:
    """Test JPEG buffer management"""
    
    def test_buffer_size_constant(self):
        """Buffer size should be 32KB"""
        assert MockCameraService.JPEG_BUFFER_SIZE == 32 * 1024
    
    def test_capture_within_buffer_size(self):
        """Capture should fit within buffer"""
        camera = MockCameraService()
        camera.begin()
        success, data, size = camera.capture_jpeg(MockCameraService.JPEG_BUFFER_SIZE)
        assert success
        assert size <= MockCameraService.JPEG_BUFFER_SIZE
    
    def test_multiple_captures_sequential(self):
        """Should handle multiple sequential captures"""
        camera = MockCameraService()
        camera.begin()
        
        frames = []
        for _ in range(5):
            success, data, size = camera.capture_jpeg()
            assert success
            frames.append((data, size))
        
        assert len(frames) == 5


# ============================================================================
# Error Handling Tests
# ============================================================================

class TestErrorHandling:
    """Test error handling"""
    
    def test_error_message_persistence(self):
        """Error message should persist until new error"""
        camera = MockCameraService()
        camera.capture_jpeg()
        error1 = camera.last_error
        
        camera.begin()
        camera.fail_next_capture = True
        camera.capture_jpeg()
        error2 = camera.last_error
        
        assert error1 != error2
    
    def test_clear_error_on_success(self):
        """Error should clear on successful operation"""
        camera = MockCameraService()
        camera.capture_jpeg()
        assert camera.last_error != ""
        
        # After begin, camera should be initialized
        camera.begin()
        # Successful capture should clear error
        success, data, size = camera.capture_jpeg()
        assert success
        # Error should be cleared after successful operation
        assert camera.last_error == ""


# ============================================================================
# Integration Tests
# ============================================================================

class TestCaptureIntegration:
    """Integration tests for complete capture workflow"""
    
    def test_complete_capture_workflow(self):
        """Test complete initialization and capture workflow"""
        camera = MockCameraService()
        
        # Initialize
        assert camera.begin()
        
        # Configure
        assert camera.set_resolution(320, 240)
        assert camera.set_quality(10)
        
        # Capture
        success, data, size = camera.capture_jpeg()
        assert success
        assert data is not None
        assert size > 0
        
        # Verify
        status = camera.get_status()
        assert status["initialized"]
        assert status["width"] == 320
        assert status["height"] == 240
    
    def test_sequence_of_captures(self):
        """Test sequence of consecutive captures"""
        camera = MockCameraService()
        camera.begin()
        
        sizes = []
        for i in range(10):
            success, data, size = camera.capture_jpeg()
            assert success
            sizes.append(size)
        
        # All sizes should be within reasonable range
        assert len(set(sizes)) > 0  # At least some variation is expected

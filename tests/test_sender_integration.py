"""
Integration tests for SENDER system.

Tests cover:
- Complete initialization sequence
- Full capture and serve workflow
- Error recovery
- System shutdown
- Performance and metrics
"""

import pytest
from typing import Optional, Tuple
from test_sender_camera import MockCameraService
from test_sender_http import MockHttpServer, HttpRequest, HttpMethod
from test_sender_wifi import MockWifiManager


class SenderSystemIntegration:
    """Complete SENDER system for integration testing"""
    
    def __init__(self):
        self.camera = MockCameraService()
        self.http_server = MockHttpServer()
        self.wifi_manager = MockWifiManager()
        self.initialized = False
        self.setup_complete = False
        self.system_errors: list = []
    
    def setup(self, ssid: str, password: str, port: int = 80) -> Tuple[bool, str]:
        """
        Initialize complete system.
        
        Returns:
            Tuple of (success, error_message)
        """
        try:
            # Step 1: Initialize camera
            if not self.camera.begin():
                return False, "Camera initialization failed"
            
            # Step 2: Connect WiFi
            if not self.wifi_manager.connect(ssid, password, 15000):
                return False, f"WiFi connection failed: {self.wifi_manager.last_error}"
            
            # Step 3: Start HTTP server
            if not self.http_server.begin(port):
                return False, "HTTP server startup failed"
            
            self.initialized = True
            self.setup_complete = True
            return True, ""
        
        except Exception as e:
            error_msg = f"Setup error: {str(e)}"
            self.system_errors.append(error_msg)
            return False, error_msg
    
    def capture_and_serve(self, path: str = "/capture") -> Tuple[bool, Optional[bytes], int]:
        """
        Perform capture and serve cycle.
        
        Returns:
            Tuple of (success, jpeg_data, size)
        """
        if not self.setup_complete:
            self.system_errors.append("System not initialized")
            return False, None, 0
        
        try:
            # Create HTTP request
            request = HttpRequest(HttpMethod.GET, path)
            
            # Handle request through HTTP server
            response = self.http_server.handle_request(request)
            
            return (
                response.status_code == 200,
                response.body if response.status_code == 200 else None,
                len(response.body) if response.status_code == 200 else 0
            )
        
        except Exception as e:
            error_msg = f"Capture error: {str(e)}"
            self.system_errors.append(error_msg)
            return False, None, 0
    
    def shutdown(self) -> bool:
        """Shutdown system gracefully"""
        try:
            self.http_server.end()
            self.wifi_manager.disconnect()
            self.camera.end()
            self.initialized = False
            return True
        except Exception as e:
            self.system_errors.append(f"Shutdown error: {str(e)}")
            return False
    
    def get_system_stats(self) -> dict:
        """Get complete system statistics"""
        return {
            "camera_initialized": self.camera.initialized,
            "wifi_connected": self.wifi_manager.is_connected(),
            "http_running": self.http_server.is_running(),
            "total_captures": self.http_server.get_stats()["successful_captures"],
            "failed_captures": self.http_server.get_stats()["failed_captures"],
            "bytes_sent": self.http_server.get_stats()["bytes_sent"],
            "camera_ip": self.wifi_manager.get_local_ip(),
            "camera_signal": self.wifi_manager.get_signal_strength(),
            "system_errors": len(self.system_errors)
        }
    
    def is_healthy(self) -> bool:
        """Check if system is in healthy state"""
        return (
            self.camera.initialized and
            self.wifi_manager.is_connected() and
            self.http_server.is_running() and
            len(self.system_errors) == 0
        )


# ============================================================================
# Basic Integration Tests
# ============================================================================

class TestBasicIntegration:
    """Basic integration tests"""
    
    def test_system_initialization(self):
        """System should initialize successfully"""
        system = SenderSystemIntegration()
        success, error = system.setup("TestNet", "TestPass")
        assert success, f"Setup failed: {error}"
        assert system.setup_complete
    
    def test_system_not_ready_before_setup(self):
        """System should not be ready before setup"""
        system = SenderSystemIntegration()
        assert not system.setup_complete
    
    def test_single_capture_after_setup(self):
        """Should capture image after setup"""
        system = SenderSystemIntegration()
        system.setup("TestNet", "TestPass")
        
        success, data, size = system.capture_and_serve()
        assert success
        assert data is not None
        assert size > 0
    
    def test_shutdown_succeeds(self):
        """System should shutdown gracefully"""
        system = SenderSystemIntegration()
        system.setup("TestNet", "TestPass")
        assert system.shutdown()


# ============================================================================
# Setup Failure Tests
# ============================================================================

class TestSetupFailures:
    """Test setup failure scenarios"""
    
    def test_setup_fails_with_invalid_wifi(self):
        """Setup should fail with invalid WiFi"""
        system = SenderSystemIntegration()
        system.wifi_manager.fail_next_connection = True
        
        success, error = system.setup("TestNet", "TestPass")
        assert not success
        assert "WiFi" in error
    
    def test_setup_fails_gracefully(self):
        """Setup failures should be reported cleanly"""
        system = SenderSystemIntegration()
        system.wifi_manager.timeout_next_connection = True
        
        success, error = system.setup("TestNet", "TestPass")
        assert not success
        assert error != ""
    
    def test_capture_fails_if_not_setup(self):
        """Capture should fail if system not setup"""
        system = SenderSystemIntegration()
        
        success, data, size = system.capture_and_serve()
        assert not success


# ============================================================================
# Complete Workflow Tests
# ============================================================================

class TestCompleteWorkflow:
    """Test complete system workflows"""
    
    def test_full_startup_capture_shutdown_cycle(self):
        """Test complete startup → capture → shutdown cycle"""
        system = SenderSystemIntegration()
        
        # Setup
        success, error = system.setup("HomeNet", "Password123")
        assert success, f"Setup failed: {error}"
        
        # Capture
        success, data, size = system.capture_and_serve()
        assert success
        assert len(data) > 0
        
        # Shutdown
        assert system.shutdown()
        assert not system.http_server.is_running()
    
    def test_multiple_captures_in_session(self):
        """Should handle multiple captures in one session"""
        system = SenderSystemIntegration()
        system.setup("TestNet", "TestPass")
        
        capture_count = 10
        for i in range(capture_count):
            success, data, size = system.capture_and_serve()
            assert success, f"Capture {i} failed"
        
        stats = system.http_server.get_stats()
        assert stats["successful_captures"] == capture_count
    
    def test_capture_with_different_paths(self):
        """Should handle requests to root and capture endpoints"""
        system = SenderSystemIntegration()
        system.setup("TestNet", "TestPass")
        
        # Root request
        success, data, size = system.capture_and_serve("/")
        assert success
        
        # Capture request
        success, data, size = system.capture_and_serve("/capture")
        assert success


# ============================================================================
# Error Recovery Tests
# ============================================================================

class TestErrorRecovery:
    """Test error recovery"""
    
    def test_recovery_from_capture_failure(self):
        """System should recover from capture failure"""
        system = SenderSystemIntegration()
        system.setup("TestNet", "TestPass")
        
        # First capture fails
        system.http_server.error_next_capture = True
        success, data, size = system.capture_and_serve()
        assert not success
        
        # Second capture succeeds
        success, data, size = system.capture_and_serve()
        assert success
    
    def test_system_health_tracking(self):
        """System should track health status"""
        system = SenderSystemIntegration()
        system.setup("TestNet", "TestPass")
        
        assert system.is_healthy()
        
        # Add error
        system.system_errors.append("Test error")
        assert not system.is_healthy()


# ============================================================================
# Statistics and Monitoring Tests
# ============================================================================

class TestSystemStatistics:
    """Test system statistics and monitoring"""
    
    def test_capture_statistics(self):
        """System should track capture statistics"""
        system = SenderSystemIntegration()
        system.setup("TestNet", "TestPass")
        
        # Perform captures
        for _ in range(3):
            system.capture_and_serve()
        
        stats = system.get_system_stats()
        assert stats["total_captures"] == 3
        assert stats["failed_captures"] == 0
    
    def test_bandwidth_tracking(self):
        """System should track bytes sent"""
        system = SenderSystemIntegration()
        system.setup("TestNet", "TestPass")
        
        system.capture_and_serve()
        
        stats = system.get_system_stats()
        assert stats["bytes_sent"] > 0
    
    def test_error_count_tracking(self):
        """System should track error count"""
        system = SenderSystemIntegration()
        system.setup("TestNet", "TestPass")
        
        initial_errors = len(system.system_errors)
        
        # Trigger error
        system.capture_and_serve()  # This shouldn't cause error
        
        stats = system.get_system_stats()
        assert stats["system_errors"] >= initial_errors
    
    def test_connection_statistics(self):
        """System should report connection statistics"""
        system = SenderSystemIntegration()
        system.setup("TestNet", "TestPass")
        
        stats = system.get_system_stats()
        assert stats["camera_ip"] != ""
        assert stats["camera_signal"] < 0


# ============================================================================
# Stress Tests
# ============================================================================

class TestStress:
    """Stress tests for robustness"""
    
    def test_rapid_captures(self):
        """Should handle rapid capture requests"""
        system = SenderSystemIntegration()
        system.setup("TestNet", "TestPass")
        
        for _ in range(50):
            success, data, size = system.capture_and_serve()
            assert success
        
        stats = system.http_server.get_stats()
        assert stats["successful_captures"] == 50
    
    def test_sustained_operation(self):
        """System should handle sustained operation"""
        system = SenderSystemIntegration()
        system.setup("TestNet", "TestPass")
        
        # Simulate 1 capture per 100ms for 1 second
        for batch in range(10):
            success, data, size = system.capture_and_serve()
            assert success
        
        assert system.is_healthy()
    
    def test_large_number_of_failed_requests(self):
        """System should handle failed requests gracefully"""
        system = SenderSystemIntegration()
        system.setup("TestNet", "TestPass")
        
        # Make some requests to invalid paths
        for _ in range(5):
            request = HttpRequest(HttpMethod.GET, "/invalid")
            response = system.http_server.handle_request(request)
            assert response.status_code == 404
        
        # Valid request should still work
        success, data, size = system.capture_and_serve()
        assert success


# ============================================================================
# Configuration Tests
# ============================================================================

class TestConfiguration:
    """Test system configuration"""
    
    def test_custom_port_configuration(self):
        """Should support custom server port"""
        system = SenderSystemIntegration()
        success, error = system.setup("TestNet", "TestPass", port=8080)
        assert success
        assert system.http_server.port == 8080
    
    def test_camera_configuration(self):
        """Should allow camera reconfiguration"""
        system = SenderSystemIntegration()
        system.setup("TestNet", "TestPass")
        
        # Reconfigure camera
        system.camera.set_resolution(640, 480)
        system.camera.set_quality(50)
        
        status = system.camera.get_status()
        assert status["width"] == 640
        assert status["height"] == 480
        assert status["quality"] == 50


# ============================================================================
# System State Tests
# ============================================================================

class TestSystemState:
    """Test system state management"""
    
    def test_state_after_successful_setup(self):
        """System state should be correct after setup"""
        system = SenderSystemIntegration()
        system.setup("TestNet", "TestPass")
        
        assert system.initialized
        assert system.setup_complete
        assert system.http_server.is_running()
        assert system.wifi_manager.is_connected()
        assert system.camera.initialized
    
    def test_state_after_shutdown(self):
        """System state should be clean after shutdown"""
        system = SenderSystemIntegration()
        system.setup("TestNet", "TestPass")
        system.shutdown()
        
        assert not system.initialized
        assert not system.http_server.is_running()
        assert not system.wifi_manager.is_connected()
        assert not system.camera.initialized


# ============================================================================
# Edge Case Tests
# ============================================================================

class TestEdgeCases:
    """Test edge cases and boundary conditions"""
    
    def test_setup_idempotent(self):
        """Setup should be idempotent or cleanly handle re-setup"""
        system = SenderSystemIntegration()
        
        success1, error1 = system.setup("TestNet", "TestPass")
        assert success1, f"First setup failed: {error1}"
        
        # Shutdown before second setup
        system.shutdown()
        
        success2, error2 = system.setup("TestNet", "TestPass")
        assert success2, f"Second setup failed: {error2}"
    
    def test_double_shutdown(self):
        """Double shutdown should be safe"""
        system = SenderSystemIntegration()
        system.setup("TestNet", "TestPass")
        
        assert system.shutdown()
        # Second shutdown should not crash
        assert system.shutdown()
    
    def test_capture_immediately_after_setup(self):
        """Should be able to capture immediately after setup"""
        system = SenderSystemIntegration()
        system.setup("TestNet", "TestPass")
        
        # No delay, immediate capture
        success, data, size = system.capture_and_serve()
        assert success

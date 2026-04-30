"""
Pytest module for SENDER HTTP server service testing.

Tests cover:
- HTTP request handling
- JPEG response generation and sending
- Error responses
- Server statistics and metrics
- Concurrent request handling
"""

import pytest
from typing import Dict, List, Optional, Tuple
from dataclasses import dataclass, field
from enum import Enum


class HttpMethod(Enum):
    """HTTP methods"""
    GET = "GET"
    POST = "POST"
    HEAD = "HEAD"


@dataclass
class HttpRequest:
    """Mock HTTP request"""
    method: HttpMethod
    path: str
    headers: Dict[str, str] = field(default_factory=dict)
    body: bytes = b""


@dataclass
class HttpResponse:
    """Mock HTTP response"""
    status_code: int
    headers: Dict[str, str] = field(default_factory=dict)
    body: bytes = b""
    
    @property
    def status_line(self) -> str:
        """Get HTTP status line"""
        status_messages = {
            200: "OK",
            400: "Bad Request",
            404: "Not Found",
            500: "Internal Server Error"
        }
        return f"HTTP/1.1 {self.status_code} {status_messages.get(self.status_code, 'Unknown')}"


class MockJpegData:
    """Mock JPEG data generator"""
    
    SOI = bytes([0xFF, 0xD8])
    EOI = bytes([0xFF, 0xD9])
    
    @staticmethod
    def create_jpeg(size_bytes: int) -> bytes:
        """Create mock JPEG data"""
        if size_bytes < 4:
            return MockJpegData.SOI + MockJpegData.EOI
        
        padding_size = size_bytes - 4
        return MockJpegData.SOI + b'\x00' * padding_size + MockJpegData.EOI


class MockHttpServer:
    """Mock HTTP server for testing SENDER logic"""
    
    MAX_JPEG_SIZE = 32 * 1024  # 32KB
    
    def __init__(self):
        self.running = False
        self.port = 0
        self.total_requests = 0
        self.successful_captures = 0
        self.failed_captures = 0
        self.request_history: List[Tuple[HttpRequest, HttpResponse]] = []
        self.last_response_time_ms = 0
        self.requests_by_path: Dict[str, int] = {}
        self.bytes_sent = 0
        self.error_next_capture = False
    
    def begin(self, port: int = 80) -> bool:
        """Start HTTP server"""
        if self.running:
            return False
        self.port = port
        self.running = True
        return True
    
    def end(self):
        """Stop HTTP server"""
        self.running = False
    
    def is_running(self) -> bool:
        """Check if server is running"""
        return self.running
    
    def handle_request(self, request: HttpRequest) -> HttpResponse:
        """
        Handle HTTP request and generate response.
        
        Args:
            request: HTTP request to handle
            
        Returns:
            HTTP response
        """
        if not self.running:
            return HttpResponse(500, {}, b"Server not running")
        
        self.total_requests += 1
        self.requests_by_path[request.path] = self.requests_by_path.get(request.path, 0) + 1
        
        if request.method != HttpMethod.GET:
            response = HttpResponse(400, {"Content-Type": "text/plain"}, b"Method not allowed")
        elif request.path == "/":
            response = self._handle_root()
        elif request.path == "/capture":
            response = self._handle_capture()
        else:
            response = HttpResponse(404, {"Content-Type": "text/plain"}, b"Not Found")
        
        self.request_history.append((request, response))
        return response
    
    def _handle_root(self) -> HttpResponse:
        """Handle GET / request"""
        html = b"<html><body><h1>ESP32-CAM Server</h1></body></html>"
        return HttpResponse(
            200,
            {"Content-Type": "text/html", "Content-Length": str(len(html))},
            html
        )
    
    def _handle_capture(self) -> HttpResponse:
        """Handle GET /capture request"""
        if self.error_next_capture:
            self.error_next_capture = False
            self.failed_captures += 1
            return HttpResponse(500, {}, b"Capture failed")
        
        # Simulate JPEG capture
        jpeg_size = 8192  # 8KB typical
        jpeg_data = MockJpegData.create_jpeg(jpeg_size)
        
        self.successful_captures += 1
        self.bytes_sent += len(jpeg_data)
        self.last_response_time_ms = 50  # Simulate 50ms response
        
        return HttpResponse(
            200,
            {
                "Content-Type": "image/jpeg",
                "Content-Length": str(len(jpeg_data))
            },
            jpeg_data
        )
    
    def get_stats(self) -> Dict:
        """Get server statistics"""
        avg_response_time = (
            self.last_response_time_ms if self.successful_captures > 0 else 0
        )
        return {
            "total_requests": self.total_requests,
            "successful_captures": self.successful_captures,
            "failed_captures": self.failed_captures,
            "average_response_time_ms": avg_response_time,
            "bytes_sent": self.bytes_sent
        }


# ============================================================================
# Initialization Tests
# ============================================================================

class TestHttpServerInitialization:
    """Test HTTP server initialization"""
    
    def test_server_starts_not_running(self):
        """Server should not be running on construction"""
        server = MockHttpServer()
        assert not server.is_running()
    
    def test_begin_starts_server(self):
        """Begin should start server"""
        server = MockHttpServer()
        assert server.begin()
        assert server.is_running()
    
    def test_begin_sets_port(self):
        """Begin should set port"""
        server = MockHttpServer()
        server.begin(8080)
        assert server.port == 8080
    
    def test_double_begin_fails(self):
        """Begin on already running server should fail"""
        server = MockHttpServer()
        assert server.begin()
        assert not server.begin()  # Should fail
    
    def test_end_stops_server(self):
        """End should stop server"""
        server = MockHttpServer()
        server.begin()
        server.end()
        assert not server.is_running()


# ============================================================================
# Request Handling Tests
# ============================================================================

class TestRootRequest:
    """Test root endpoint handling"""
    
    def test_root_path_returns_html(self):
        """GET / should return HTML content"""
        server = MockHttpServer()
        server.begin()
        
        request = HttpRequest(HttpMethod.GET, "/")
        response = server.handle_request(request)
        
        assert response.status_code == 200
        assert "text/html" in response.headers["Content-Type"]
        assert b"ESP32-CAM" in response.body
    
    def test_root_path_increments_request_count(self):
        """Handling root should increment request count"""
        server = MockHttpServer()
        server.begin()
        
        request = HttpRequest(HttpMethod.GET, "/")
        server.handle_request(request)
        
        assert server.total_requests == 1


class TestCaptureRequest:
    """Test capture endpoint"""
    
    def test_capture_returns_jpeg(self):
        """GET /capture should return JPEG"""
        server = MockHttpServer()
        server.begin()
        
        request = HttpRequest(HttpMethod.GET, "/capture")
        response = server.handle_request(request)
        
        assert response.status_code == 200
        assert response.headers["Content-Type"] == "image/jpeg"
        assert len(response.body) > 0
    
    def test_capture_has_valid_jpeg_markers(self):
        """Captured image should have JPEG markers"""
        server = MockHttpServer()
        server.begin()
        
        request = HttpRequest(HttpMethod.GET, "/capture")
        response = server.handle_request(request)
        
        # Check SOI marker
        assert response.body[:2] == bytes([0xFF, 0xD8])
        # Check EOI marker
        assert response.body[-2:] == bytes([0xFF, 0xD9])
    
    def test_capture_increments_success_count(self):
        """Successful capture should increment counter"""
        server = MockHttpServer()
        server.begin()
        
        request = HttpRequest(HttpMethod.GET, "/capture")
        server.handle_request(request)
        
        stats = server.get_stats()
        assert stats["successful_captures"] == 1
    
    def test_capture_error_increments_failure_count(self):
        """Failed capture should increment failure counter"""
        server = MockHttpServer()
        server.begin()
        server.error_next_capture = True
        
        request = HttpRequest(HttpMethod.GET, "/capture")
        response = server.handle_request(request)
        
        assert response.status_code == 500
        
        stats = server.get_stats()
        assert stats["failed_captures"] == 1
    
    def test_capture_response_time_tracked(self):
        """Response time should be tracked"""
        server = MockHttpServer()
        server.begin()
        
        request = HttpRequest(HttpMethod.GET, "/capture")
        server.handle_request(request)
        
        stats = server.get_stats()
        assert stats["average_response_time_ms"] > 0


# ============================================================================
# Error Handling Tests
# ============================================================================

class TestErrorHandling:
    """Test error handling"""
    
    def test_invalid_method_returns_400(self):
        """Invalid HTTP method should return 400"""
        server = MockHttpServer()
        server.begin()
        
        request = HttpRequest(HttpMethod.POST, "/capture")
        response = server.handle_request(request)
        
        assert response.status_code == 400
    
    def test_not_found_returns_404(self):
        """Invalid path should return 404"""
        server = MockHttpServer()
        server.begin()
        
        request = HttpRequest(HttpMethod.GET, "/invalid")
        response = server.handle_request(request)
        
        assert response.status_code == 404
    
    def test_server_not_running_returns_error(self):
        """Request on stopped server should return error"""
        server = MockHttpServer()
        # Don't start server
        
        request = HttpRequest(HttpMethod.GET, "/capture")
        response = server.handle_request(request)
        
        assert response.status_code == 500


# ============================================================================
# Statistics Tests
# ============================================================================

class TestServerStatistics:
    """Test server statistics tracking"""
    
    def test_total_requests_count(self):
        """Should track total requests"""
        server = MockHttpServer()
        server.begin()
        
        for _ in range(5):
            request = HttpRequest(HttpMethod.GET, "/capture")
            server.handle_request(request)
        
        stats = server.get_stats()
        assert stats["total_requests"] == 5
    
    def test_successful_captures_count(self):
        """Should track successful captures"""
        server = MockHttpServer()
        server.begin()
        
        for _ in range(3):
            request = HttpRequest(HttpMethod.GET, "/capture")
            server.handle_request(request)
        
        stats = server.get_stats()
        assert stats["successful_captures"] == 3
    
    def test_bytes_sent_tracked(self):
        """Should track bytes sent"""
        server = MockHttpServer()
        server.begin()
        
        request = HttpRequest(HttpMethod.GET, "/capture")
        server.handle_request(request)
        
        stats = server.get_stats()
        assert stats["bytes_sent"] > 0
    
    def test_request_count_by_path(self):
        """Should track requests by path"""
        server = MockHttpServer()
        server.begin()
        
        for _ in range(3):
            request = HttpRequest(HttpMethod.GET, "/capture")
            server.handle_request(request)
        
        request = HttpRequest(HttpMethod.GET, "/")
        server.handle_request(request)
        
        assert server.requests_by_path["/capture"] == 3
        assert server.requests_by_path["/"] == 1


# ============================================================================
# Response Headers Tests
# ============================================================================

class TestResponseHeaders:
    """Test HTTP response headers"""
    
    def test_jpeg_response_has_content_type(self):
        """JPEG response should have Content-Type header"""
        server = MockHttpServer()
        server.begin()
        
        request = HttpRequest(HttpMethod.GET, "/capture")
        response = server.handle_request(request)
        
        assert "Content-Type" in response.headers
        assert response.headers["Content-Type"] == "image/jpeg"
    
    def test_jpeg_response_has_content_length(self):
        """JPEG response should have Content-Length header"""
        server = MockHttpServer()
        server.begin()
        
        request = HttpRequest(HttpMethod.GET, "/capture")
        response = server.handle_request(request)
        
        assert "Content-Length" in response.headers
    
    def test_html_response_has_correct_type(self):
        """HTML response should have correct Content-Type"""
        server = MockHttpServer()
        server.begin()
        
        request = HttpRequest(HttpMethod.GET, "/")
        response = server.handle_request(request)
        
        assert "text/html" in response.headers["Content-Type"]


# ============================================================================
# Concurrent Request Tests
# ============================================================================

class TestConcurrentRequests:
    """Test handling multiple consecutive requests"""
    
    def test_multiple_capture_requests(self):
        """Should handle multiple consecutive captures"""
        server = MockHttpServer()
        server.begin()
        
        for _ in range(10):
            request = HttpRequest(HttpMethod.GET, "/capture")
            response = server.handle_request(request)
            assert response.status_code == 200
        
        stats = server.get_stats()
        assert stats["total_requests"] == 10
        assert stats["successful_captures"] == 10
    
    def test_mixed_request_types(self):
        """Should handle mixed request types"""
        server = MockHttpServer()
        server.begin()
        
        # Mix of root and capture requests
        for _ in range(3):
            request = HttpRequest(HttpMethod.GET, "/")
            server.handle_request(request)
            
            request = HttpRequest(HttpMethod.GET, "/capture")
            server.handle_request(request)
        
        stats = server.get_stats()
        assert stats["total_requests"] == 6
        assert stats["successful_captures"] == 3
    
    def test_request_history_maintained(self):
        """Should maintain request history"""
        server = MockHttpServer()
        server.begin()
        
        for i in range(3):
            request = HttpRequest(HttpMethod.GET, "/capture")
            server.handle_request(request)
        
        assert len(server.request_history) == 3


# ============================================================================
# Integration Tests
# ============================================================================

class TestHttpServerIntegration:
    """Integration tests for HTTP server"""
    
    def test_complete_server_lifecycle(self):
        """Test complete server lifecycle"""
        server = MockHttpServer()
        
        # Start
        assert server.begin(80)
        assert server.is_running()
        
        # Handle requests
        for _ in range(5):
            request = HttpRequest(HttpMethod.GET, "/capture")
            response = server.handle_request(request)
            assert response.status_code == 200
        
        # Check stats
        stats = server.get_stats()
        assert stats["total_requests"] == 5
        
        # Stop
        server.end()
        assert not server.is_running()
    
    def test_typical_client_session(self):
        """Simulate typical client interaction"""
        server = MockHttpServer()
        server.begin()
        
        # Client requests root
        request = HttpRequest(HttpMethod.GET, "/")
        response = server.handle_request(request)
        assert response.status_code == 200
        
        # Client downloads image
        request = HttpRequest(HttpMethod.GET, "/capture")
        response = server.handle_request(request)
        assert response.status_code == 200
        assert response.headers["Content-Type"] == "image/jpeg"
        
        # Client requests another image
        request = HttpRequest(HttpMethod.GET, "/capture")
        response = server.handle_request(request)
        assert response.status_code == 200

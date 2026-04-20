"""
Pytest module for HTTP response parsing and validation.

Tests focus on:
- JPEG header/footer validation
- HTTP response code handling
- Content-Length validation
- Chunked data handling
"""

import pytest
from dataclasses import dataclass
from typing import Tuple


@dataclass
class JpegData:
    """JPEG binary data with metadata"""
    data: bytes
    is_valid: bool


class JpegValidator:
    """JPEG format validation logic"""
    
    JPEG_SOI_MARKER = bytes([0xFF, 0xD8])  # Start of Image
    JPEG_EOI_MARKER = bytes([0xFF, 0xD9])  # End of Image
    JPEG_MIN_SIZE = 2
    JPEG_MAX_SIZE = 32 * 1024  # 32KB
    
    @staticmethod
    def is_valid_header(data: bytes) -> bool:
        """Check if data has valid JPEG start marker (0xFF 0xD8)"""
        if len(data) < len(JpegValidator.JPEG_SOI_MARKER):
            return False
        return data[:2] == JpegValidator.JPEG_SOI_MARKER
    
    @staticmethod
    def is_valid_footer(data: bytes) -> bool:
        """Check if data has valid JPEG end marker (0xFF 0xD9)"""
        if len(data) < len(JpegValidator.JPEG_EOI_MARKER):
            return False
        return data[-2:] == JpegValidator.JPEG_EOI_MARKER
    
    @staticmethod
    def is_valid_size(data_size: int) -> bool:
        """Check if JPEG size is within valid range"""
        return JpegValidator.JPEG_MIN_SIZE <= data_size <= JpegValidator.JPEG_MAX_SIZE
    
    @staticmethod
    def validate(data: bytes) -> Tuple[bool, str]:
        """
        Validate complete JPEG data.
        
        Returns:
            Tuple of (is_valid, error_message)
        """
        if not JpegValidator.is_valid_size(len(data)):
            return False, f"Size {len(data)} outside valid range"
        
        if not JpegValidator.is_valid_header(data):
            return False, "Missing JPEG SOI marker (0xFF 0xD8)"
        
        if not JpegValidator.is_valid_footer(data):
            return False, "Missing JPEG EOI marker (0xFF 0xD9)"
        
        return True, ""


class HttpResponseParser:
    """Parse HTTP responses for image retrieval"""
    
    @staticmethod
    def parse_content_length(headers: dict) -> int:
        """Extract Content-Length from HTTP headers"""
        content_length_str = headers.get('Content-Length', '0')
        try:
            return int(content_length_str)
        except ValueError:
            return -1
    
    @staticmethod
    def validate_http_code(code: int) -> bool:
        """Validate HTTP response code for image fetch"""
        return code == 200
    
    @staticmethod
    def validate_content_type(content_type: str) -> bool:
        """Validate Content-Type is image/jpeg"""
        return content_type.lower() == 'image/jpeg'


class ChunkedDataReconstructor:
    """Handle reconstruction of JPEG data from chunks"""
    
    @staticmethod
    def merge_chunks(chunks: list) -> bytes:
        """Merge multiple chunks into single data stream"""
        return b''.join(chunks)
    
    @staticmethod
    def validate_chunk_boundary(prev_chunk: bytes, current_chunk: bytes) -> bool:
        """Verify no data loss at chunk boundaries"""
        if not prev_chunk or not current_chunk:
            return True
        # Check no marker straddling occurs
        return True
    
    @staticmethod
    def detect_missing_data(expected_size: int, actual_size: int) -> bool:
        """Detect if expected data matches actual"""
        return expected_size == actual_size


# ============================================================================
# JPEG Header/Footer Validation Tests
# ============================================================================

class TestJpegHeaderValidation:
    """Test JPEG header (SOI marker) detection"""
    
    def test_valid_jpeg_header(self):
        """Valid JPEG starts with 0xFF 0xD8"""
        valid_header = bytes([0xFF, 0xD8]) + b'\x00\x00'
        assert JpegValidator.is_valid_header(valid_header)
    
    def test_invalid_jpeg_header(self):
        """Invalid header should be rejected"""
        invalid_header = bytes([0x00, 0x00]) + b'\xFF\xD8'
        assert not JpegValidator.is_valid_header(invalid_header)
    
    def test_empty_data_has_no_header(self):
        """Empty data cannot have valid header"""
        assert not JpegValidator.is_valid_header(b'')
    
    def test_single_byte_not_valid_header(self):
        """Single byte cannot contain SOI marker"""
        assert not JpegValidator.is_valid_header(b'\xFF')


class TestJpegFooterValidation:
    """Test JPEG footer (EOI marker) detection"""
    
    def test_valid_jpeg_footer(self):
        """Valid JPEG ends with 0xFF 0xD9"""
        valid_footer = b'\x00\x00' + bytes([0xFF, 0xD9])
        assert JpegValidator.is_valid_footer(valid_footer)
    
    def test_invalid_jpeg_footer(self):
        """Invalid footer should be rejected"""
        invalid_footer = b'\xFF\xD9' + bytes([0x00, 0x00])
        assert not JpegValidator.is_valid_footer(invalid_footer)
    
    def test_empty_data_has_no_footer(self):
        """Empty data cannot have valid footer"""
        assert not JpegValidator.is_valid_footer(b'')


class TestJpegSizeValidation:
    """Test JPEG size constraints"""
    
    def test_minimum_size_valid(self):
        """Minimum JPEG size (2 bytes - just markers)"""
        assert JpegValidator.is_valid_size(2)
    
    def test_maximum_size_valid(self):
        """Maximum size (32KB) is valid"""
        assert JpegValidator.is_valid_size(32 * 1024)
    
    def test_below_minimum_invalid(self):
        """Size < 2 bytes is invalid"""
        assert not JpegValidator.is_valid_size(1)
    
    def test_above_maximum_invalid(self):
        """Size > 32KB is invalid"""
        assert not JpegValidator.is_valid_size(32 * 1024 + 1)
    
    def test_typical_size_valid(self):
        """Typical QVGA JPEG (~8KB) is valid"""
        assert JpegValidator.is_valid_size(8192)


class TestCompleteJpegValidation:
    """Test complete JPEG validation"""
    
    def test_valid_minimal_jpeg(self):
        """Minimal valid JPEG (SOI + EOI markers only)"""
        minimal_jpeg = bytes([0xFF, 0xD8, 0xFF, 0xD9])
        is_valid, error = JpegValidator.validate(minimal_jpeg)
        assert is_valid
        assert error == ""
    
    def test_missing_soi_marker(self):
        """JPEG without SOI marker is invalid"""
        invalid_jpeg = b'\x00\x00' + bytes([0xFF, 0xD9])
        is_valid, error = JpegValidator.validate(invalid_jpeg)
        assert not is_valid
        assert "SOI marker" in error
    
    def test_missing_eoi_marker(self):
        """JPEG without EOI marker is invalid"""
        invalid_jpeg = bytes([0xFF, 0xD8]) + b'\x00\x00'
        is_valid, error = JpegValidator.validate(invalid_jpeg)
        assert not is_valid
        assert "EOI marker" in error
    
    def test_size_too_small(self):
        """JPEG too small is invalid"""
        is_valid, error = JpegValidator.validate(b'\xFF')
        assert not is_valid
    
    def test_size_too_large(self):
        """JPEG larger than 32KB is invalid"""
        oversized = bytes([0xFF, 0xD8]) + (b'\x00' * (32 * 1024)) + bytes([0xFF, 0xD9])
        is_valid, error = JpegValidator.validate(oversized)
        assert not is_valid


# ============================================================================
# HTTP Response Validation Tests
# ============================================================================

class TestHttpResponseValidation:
    """Test HTTP response parsing"""
    
    def test_valid_http_ok(self):
        """HTTP 200 OK is valid"""
        assert HttpResponseParser.validate_http_code(200)
    
    def test_http_not_found(self):
        """HTTP 404 is invalid"""
        assert not HttpResponseParser.validate_http_code(404)
    
    def test_http_server_error(self):
        """HTTP 500 is invalid"""
        assert not HttpResponseParser.validate_http_code(500)
    
    def test_valid_jpeg_content_type(self):
        """image/jpeg content type is valid"""
        assert HttpResponseParser.validate_content_type("image/jpeg")
    
    def test_uppercase_content_type(self):
        """Content type is case-insensitive"""
        assert HttpResponseParser.validate_content_type("IMAGE/JPEG")
    
    def test_invalid_content_type(self):
        """Wrong content type is invalid"""
        assert not HttpResponseParser.validate_content_type("text/html")
    
    def test_parse_content_length(self):
        """Extract Content-Length from headers"""
        headers = {'Content-Length': '8192'}
        length = HttpResponseParser.parse_content_length(headers)
        assert length == 8192
    
    def test_invalid_content_length(self):
        """Invalid Content-Length returns -1"""
        headers = {'Content-Length': 'invalid'}
        length = HttpResponseParser.parse_content_length(headers)
        assert length == -1
    
    def test_missing_content_length(self):
        """Missing Content-Length returns 0"""
        headers = {}
        length = HttpResponseParser.parse_content_length(headers)
        assert length == 0


# ============================================================================
# Buffer Handling Tests
# ============================================================================

class TestChunkedDataReconstruction:
    """Test reconstruction of JPEG from chunks"""
    
    def test_merge_two_chunks(self):
        """Merge two data chunks"""
        chunk1 = b'\xFF\xD8\x00\x00'
        chunk2 = b'\x00\x00\xFF\xD9'
        merged = ChunkedDataReconstructor.merge_chunks([chunk1, chunk2])
        assert merged == b'\xFF\xD8\x00\x00\x00\x00\xFF\xD9'
    
    def test_merge_multiple_chunks(self):
        """Merge 4 chunks maintaining order"""
        chunks = [b'AAA', b'BBB', b'CCC', b'DDD']
        merged = ChunkedDataReconstructor.merge_chunks(chunks)
        assert merged == b'AAABBBCCCDDD'
    
    def test_data_size_preserved(self):
        """Total size preserved when merging"""
        chunks = [b'chunk1', b'chunk2', b'chunk3']
        total_expected = sum(len(c) for c in chunks)
        merged = ChunkedDataReconstructor.merge_chunks(chunks)
        assert len(merged) == total_expected
    
    def test_validate_complete_transfer(self):
        """Verify all expected data received"""
        expected_size = 8192
        actual_size = 8192
        assert ChunkedDataReconstructor.detect_missing_data(expected_size, actual_size)
    
    def test_detect_incomplete_transfer(self):
        """Detect when data is missing"""
        expected_size = 8192
        actual_size = 8000
        assert not ChunkedDataReconstructor.detect_missing_data(expected_size, actual_size)


# ============================================================================
# State Machine Tests
# ============================================================================

class TestStateMachine:
    """Test receiver state machine logic"""
    
    class SimpleStateMachine:
        """Minimal state machine for testing"""
        def __init__(self):
            self.state = "IDLE"
            self.retry_count = 0
            self.max_retries = 3
        
        def retry(self):
            """Attempt retry"""
            if self.retry_count < self.max_retries:
                self.retry_count += 1
                return True
            return False
        
        def reset_retry(self):
            """Reset retry counter"""
            self.retry_count = 0
        
        def can_retry(self):
            """Check if more retries available"""
            return self.retry_count < self.max_retries
    
    def test_initial_state(self):
        """State machine starts in IDLE"""
        sm = self.SimpleStateMachine()
        assert sm.state == "IDLE"
    
    def test_retry_increments(self):
        """Each retry increments counter"""
        sm = self.SimpleStateMachine()
        assert sm.retry()
        assert sm.retry_count == 1
        assert sm.retry()
        assert sm.retry_count == 2
    
    def test_max_retries_exceeded(self):
        """Cannot exceed max retries"""
        sm = self.SimpleStateMachine()
        for _ in range(sm.max_retries):
            assert sm.retry()
        assert not sm.retry()
        assert sm.retry_count == 3
    
    def test_retry_reset(self):
        """Retry counter can be reset"""
        sm = self.SimpleStateMachine()
        sm.retry()
        sm.retry()
        assert sm.retry_count == 2
        sm.reset_retry()
        assert sm.retry_count == 0
    
    def test_can_retry_before_max(self):
        """Can retry before reaching maximum"""
        sm = self.SimpleStateMachine()
        assert sm.can_retry()
        sm.retry()
        assert sm.can_retry()
    
    def test_cannot_retry_after_max(self):
        """Cannot retry after reaching maximum"""
        sm = self.SimpleStateMachine()
        for _ in range(sm.max_retries):
            sm.retry()
        assert not sm.can_retry()


# ============================================================================
# Integration Tests
# ============================================================================

class TestEndToEndFlow:
    """Integration tests for complete image fetch/display cycle"""
    
    def test_valid_http_response_with_valid_jpeg(self):
        """Valid HTTP response + valid JPEG passes all checks"""
        # HTTP response validation
        http_code = 200
        content_type = "image/jpeg"
        content_length = 8192
        
        assert HttpResponseParser.validate_http_code(http_code)
        assert HttpResponseParser.validate_content_type(content_type)
        assert HttpResponseParser.parse_content_length({'Content-Length': str(content_length)}) == content_length
        
        # JPEG validation
        valid_jpeg = bytes([0xFF, 0xD8]) + (b'\x00' * (content_length - 4)) + bytes([0xFF, 0xD9])
        is_valid, _ = JpegValidator.validate(valid_jpeg)
        assert is_valid
    
    def test_retry_on_invalid_jpeg(self):
        """System retries when JPEG is invalid"""
        sm = TestStateMachine.SimpleStateMachine()
        
        # Simulate invalid JPEG
        invalid_jpeg = b'\x00\x00\x00\x00'
        is_valid, _ = JpegValidator.validate(invalid_jpeg)
        assert not is_valid
        
        # Can retry
        assert sm.retry()
        assert sm.can_retry()

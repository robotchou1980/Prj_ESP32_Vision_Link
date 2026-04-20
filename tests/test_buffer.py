"""
Pytest module for buffer handling tests.

Tests focus on:
- Memory buffer allocation and management
- Boundary conditions
- Data integrity during transfers
- Buffer overflow prevention
"""

import pytest
from typing import List, Tuple


class ImageBuffer:
    """Simulates image buffer management on ESP32"""
    
    def __init__(self, max_size: int):
        """
        Initialize buffer.
        
        Args:
            max_size: Maximum buffer size in bytes
        """
        self.max_size = max_size
        self.data = bytearray()
        self.read_pos = 0
    
    def append(self, data: bytes) -> Tuple[bool, str]:
        """
        Append data to buffer.
        
        Args:
            data: Data to append
            
        Returns:
            Tuple of (success, error_message)
        """
        if len(self.data) + len(data) > self.max_size:
            return False, f"Buffer overflow: {len(self.data)} + {len(data)} > {self.max_size}"
        
        self.data.extend(data)
        return True, ""
    
    def get_free_space(self) -> int:
        """Get remaining buffer space"""
        return self.max_size - len(self.data)
    
    def is_full(self) -> bool:
        """Check if buffer is full"""
        return len(self.data) >= self.max_size
    
    def clear(self):
        """Clear buffer contents"""
        self.data.clear()
        self.read_pos = 0
    
    def get_size(self) -> int:
        """Get current buffer size"""
        return len(self.data)
    
    def get_data(self) -> bytes:
        """Get buffer contents as bytes"""
        return bytes(self.data)


class JpegStreamParser:
    """Parse JPEG stream in chunks"""
    
    JPEG_SOI = bytes([0xFF, 0xD8])
    JPEG_EOI = bytes([0xFF, 0xD9])
    CHUNK_SIZE = 1024
    
    def __init__(self, max_size: int = 32 * 1024):
        self.buffer = ImageBuffer(max_size)
        self.soi_found = False
        self.eoi_found = False
        self.complete = False
    
    def process_chunk(self, chunk: bytes) -> Tuple[bool, str]:
        """
        Process a single data chunk.
        
        Args:
            chunk: Data chunk to process
            
        Returns:
            Tuple of (complete, error)
        """
        # Check for overflow
        success, error = self.buffer.append(chunk)
        if not success:
            return False, error
        
        # Check for SOI marker
        if not self.soi_found and self.JPEG_SOI in chunk:
            self.soi_found = True
        
        # Check for EOI marker
        if not self.eoi_found and self.JPEG_EOI in chunk:
            self.eoi_found = True
            self.complete = True
            return True, ""
        
        return False, ""
    
    def is_complete(self) -> bool:
        """Check if JPEG is complete"""
        return self.complete and self.soi_found and self.eoi_found
    
    def has_soi(self) -> bool:
        """Check if SOI marker found"""
        return self.JPEG_SOI in self.buffer.get_data()
    
    def has_eoi(self) -> bool:
        """Check if EOI marker found"""
        return self.JPEG_EOI in self.buffer.get_data()
    
    def get_buffer(self) -> ImageBuffer:
        """Get internal buffer"""
        return self.buffer


class MemoryAllocator:
    """Simulate ESP32 memory management"""
    
    def __init__(self, total_size: int):
        self.total_size = total_size
        self.allocated = 0
        self.allocations = {}
    
    def allocate(self, name: str, size: int) -> Tuple[bool, str]:
        """
        Allocate memory block.
        
        Args:
            name: Allocation name/identifier
            size: Size in bytes
            
        Returns:
            Tuple of (success, error_message)
        """
        if self.allocated + size > self.total_size:
            available = self.total_size - self.allocated
            return False, f"Insufficient memory: need {size}, have {available}"
        
        self.allocated += size
        self.allocations[name] = size
        return True, ""
    
    def deallocate(self, name: str) -> bool:
        """Deallocate memory block"""
        if name in self.allocations:
            self.allocated -= self.allocations[name]
            del self.allocations[name]
            return True
        return False
    
    def get_free(self) -> int:
        """Get free memory"""
        return self.total_size - self.allocated
    
    def get_allocated(self) -> int:
        """Get allocated memory"""
        return self.allocated
    
    def is_fragmented(self) -> bool:
        """Check if memory fragmentation present"""
        # Simplified: consider fragmented if < 50% free
        return self.get_free() < self.total_size * 0.5


class CircularBuffer:
    """Ring buffer for streaming data"""
    
    def __init__(self, size: int):
        self.size = size
        self.buffer = bytearray(size)
        self.write_pos = 0
        self.read_pos = 0
        self.count = 0
    
    def write(self, data: bytes) -> int:
        """
        Write data to buffer.
        
        Args:
            data: Data to write
            
        Returns:
            Number of bytes written
        """
        bytes_written = 0
        for byte in data:
            if self.count < self.size:
                self.buffer[self.write_pos] = byte
                self.write_pos = (self.write_pos + 1) % self.size
                self.count += 1
                bytes_written += 1
            else:
                # Buffer full
                break
        return bytes_written
    
    def read(self, count: int) -> bytes:
        """
        Read data from buffer.
        
        Args:
            count: Number of bytes to read
            
        Returns:
            Data read
        """
        result = bytearray()
        bytes_to_read = min(count, self.count)
        for _ in range(bytes_to_read):
            result.append(self.buffer[self.read_pos])
            self.read_pos = (self.read_pos + 1) % self.size
            self.count -= 1
        return bytes(result)
    
    def get_available(self) -> int:
        """Get available data"""
        return self.count
    
    def get_free(self) -> int:
        """Get free space"""
        return self.size - self.count


# ============================================================================
# Buffer Allocation Tests
# ============================================================================

class TestBufferAllocation:
    """Test JPEG buffer allocation"""
    
    def test_allocate_typical_buffer(self):
        """Allocate typical 32KB buffer"""
        buf = ImageBuffer(32 * 1024)
        assert buf.max_size == 32 * 1024
        assert buf.get_free_space() == 32 * 1024
        assert buf.get_size() == 0
    
    def test_cannot_exceed_buffer_size(self):
        """Cannot append more data than buffer size"""
        buf = ImageBuffer(1024)
        success, error = buf.append(b'\x00' * 500)
        assert success
        success, error = buf.append(b'\x00' * 600)
        assert not success
        assert "overflow" in error.lower()
    
    def test_buffer_full_detection(self):
        """Detect when buffer is full"""
        buf = ImageBuffer(100)
        assert not buf.is_full()
        buf.append(b'\x00' * 100)
        assert buf.is_full()
    
    def test_free_space_calculation(self):
        """Correct free space after partial fill"""
        buf = ImageBuffer(1000)
        buf.append(b'\x00' * 300)
        assert buf.get_free_space() == 700
    
    def test_buffer_clear(self):
        """Clear buffer resets state"""
        buf = ImageBuffer(1000)
        buf.append(b'test data')
        assert buf.get_size() == 9
        buf.clear()
        assert buf.get_size() == 0
        assert buf.get_free_space() == 1000


class TestMemoryAllocation:
    """Test memory allocation on resource-constrained ESP32"""
    
    def test_allocate_single_buffer(self):
        """Allocate single memory block"""
        mem = MemoryAllocator(300 * 1024)  # 300KB total
        success, error = mem.allocate("jpeg_buffer", 32 * 1024)
        assert success
        assert mem.get_allocated() == 32 * 1024
    
    def test_insufficient_memory(self):
        """Cannot allocate more than available"""
        mem = MemoryAllocator(50 * 1024)  # 50KB total
        success, _ = mem.allocate("large", 32 * 1024)
        assert success
        success, error = mem.allocate("extra", 30 * 1024)
        assert not success
    
    def test_deallocate_memory(self):
        """Deallocate frees memory"""
        mem = MemoryAllocator(100 * 1024)
        mem.allocate("buffer", 32 * 1024)
        assert mem.get_allocated() == 32 * 1024
        mem.deallocate("buffer")
        assert mem.get_allocated() == 0
    
    def test_multiple_allocations(self):
        """Multiple allocations track correctly"""
        mem = MemoryAllocator(100 * 1024)
        mem.allocate("buf1", 32 * 1024)
        mem.allocate("buf2", 16 * 1024)
        mem.allocate("buf3", 8 * 1024)
        assert mem.get_allocated() == 56 * 1024
    
    def test_fragmentation_detection(self):
        """Detect when fragmentation likely"""
        mem = MemoryAllocator(100 * 1024)
        mem.allocate("large", 60 * 1024)
        # 40KB free (40%), should be fragmented
        assert mem.is_fragmented()


# ============================================================================
# JPEG Stream Parsing Tests
# ============================================================================

class TestJpegStreamParser:
    """Test parsing JPEG from byte stream"""
    
    def test_find_soi_marker(self):
        """Detect JPEG start marker"""
        parser = JpegStreamParser()
        chunk = bytes([0xFF, 0xD8]) + b'data'
        parser.process_chunk(chunk)
        assert parser.has_soi()
    
    def test_find_eoi_marker(self):
        """Detect JPEG end marker"""
        parser = JpegStreamParser()
        chunk = b'data' + bytes([0xFF, 0xD9])
        success, error = parser.process_chunk(chunk)
        assert success or parser.has_eoi()
    
    def test_complete_jpeg_detection(self):
        """Detect complete JPEG"""
        parser = JpegStreamParser()
        chunk1 = bytes([0xFF, 0xD8]) + b'jpeg_data'
        chunk2 = b'more_data' + bytes([0xFF, 0xD9])
        
        parser.process_chunk(chunk1)
        success, _ = parser.process_chunk(chunk2)
        assert success
        assert parser.is_complete()
    
    def test_buffer_overflow_prevention(self):
        """Cannot exceed parser buffer"""
        parser = JpegStreamParser(max_size=100)
        # First chunk fills buffer to capacity
        success, error = parser.process_chunk(b'\x00' * 100)
        assert error == ""  # No error, just incomplete (no EOI marker)
        assert parser.buffer.is_full()
        # Second chunk should overflow
        success, error = parser.process_chunk(b'\x00' * 50)
        assert not success
        assert "overflow" in error.lower()


class TestCircularBuffer:
    """Test circular buffer for streaming"""
    
    def test_circular_write_read(self):
        """Write and read from circular buffer"""
        buf = CircularBuffer(100)
        buf.write(b'hello')
        data = buf.read(5)
        assert data == b'hello'
    
    def test_wraparound(self):
        """Buffer wraps around correctly"""
        buf = CircularBuffer(10)
        buf.write(b'12345')
        buf.read(3)  # Read first 3
        buf.write(b'67890')  # Write more
        assert buf.get_available() == 7
    
    def test_available_space(self):
        """Track available space in buffer"""
        buf = CircularBuffer(50)
        assert buf.get_free() == 50
        buf.write(b'hello')
        assert buf.get_available() == 5
        assert buf.get_free() == 45
    
    def test_buffer_overflow_handling(self):
        """Handle buffer overflow gracefully"""
        buf = CircularBuffer(20)
        written = buf.write(b'0' * 15)
        assert written == 15
        written = buf.write(b'1' * 10)
        assert written == 5  # Only 5 bytes fit
    
    def test_fifo_order(self):
        """Maintain FIFO order"""
        buf = CircularBuffer(50)
        buf.write(b'aaa')
        buf.write(b'bbb')
        buf.write(b'ccc')
        assert buf.read(9) == b'aaabbbccc'


# ============================================================================
# Data Integrity Tests
# ============================================================================

class TestDataIntegrity:
    """Test data integrity during transfer"""
    
    def test_no_data_loss_small_chunks(self):
        """No data loss with small chunks"""
        buf = ImageBuffer(10000)
        data = b'x' * 5000
        chunks = [data[i:i+100] for i in range(0, len(data), 100)]
        
        for chunk in chunks:
            success, _ = buf.append(chunk)
            assert success
        
        assert buf.get_size() == 5000
        assert buf.get_data() == data
    
    def test_no_data_loss_large_chunks(self):
        """No data loss with large chunks"""
        buf = ImageBuffer(32 * 1024)
        data = b'y' * (16 * 1024)
        chunks = [data[i:i+8192] for i in range(0, len(data), 8192)]
        
        total = 0
        for chunk in chunks:
            success, _ = buf.append(chunk)
            assert success
            total += len(chunk)
        
        assert buf.get_size() == total
    
    def test_data_corruption_detection(self):
        """Detect corrupted data"""
        buf = ImageBuffer(100)
        original = b'original_data'
        buf.append(original)
        
        retrieved = buf.get_data()
        assert retrieved == original
        assert retrieved != b'modified_data'


# ============================================================================
# Boundary Condition Tests
# ============================================================================

class TestBoundaryConditions:
    """Test edge cases and boundary conditions"""
    
    def test_buffer_exactly_full(self):
        """Fill buffer to exact capacity"""
        buf = ImageBuffer(256)
        data = b'\x00' * 256
        success, _ = buf.append(data)
        assert success
        assert buf.is_full()
    
    def test_buffer_off_by_one(self):
        """One byte over capacity fails"""
        buf = ImageBuffer(256)
        data = b'\x00' * 257
        success, error = buf.append(data)
        assert not success
    
    def test_zero_byte_append(self):
        """Appending zero bytes succeeds"""
        buf = ImageBuffer(100)
        success, _ = buf.append(b'')
        assert success
        assert buf.get_size() == 0
    
    def test_single_byte_buffer(self):
        """Can allocate 1-byte buffer"""
        buf = ImageBuffer(1)
        success, _ = buf.append(b'x')
        assert success
        assert buf.is_full()
    
    def test_max_uint32_buffer(self):
        """Handle large buffer sizes (simulated)"""
        # Simulate max reasonable ESP32 buffer
        buf = ImageBuffer(32 * 1024)
        assert buf.max_size == 32 * 1024

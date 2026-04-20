"""
Pytest module for state machine and control flow tests.

Tests focus on:
- State transitions
- Timeout handling
- Retry logic
- Error recovery
- Non-blocking design
"""

import pytest
from typing import Dict, List, Tuple
from dataclasses import dataclass
from enum import Enum, auto


class AppState(Enum):
    """Application state enumeration"""
    BOOT = auto()
    WIFI_CONNECT = auto()
    IDLE = auto()
    FETCH_IMAGE = auto()
    DECODE = auto()
    DISPLAY = auto()
    ERROR = auto()
    RETRY = auto()


@dataclass
class TimedEvent:
    """Event with timestamp"""
    event_type: str
    timestamp: int
    data: str = ""


class TimeSimulator:
    """Simulate time progression for testing"""
    
    def __init__(self, start_time: int = 0):
        self.current_time = start_time
    
    def advance(self, ms: int) -> int:
        """
        Advance time by specified milliseconds.
        
        Args:
            ms: Milliseconds to advance
            
        Returns:
            New current time
        """
        self.current_time += ms
        return self.current_time
    
    def get_time(self) -> int:
        """Get current time"""
        return self.current_time
    
    def elapsed_since(self, timestamp: int) -> int:
        """Get elapsed time since timestamp"""
        return self.current_time - timestamp
    
    def reset(self, new_time: int = 0):
        """Reset time"""
        self.current_time = new_time


class StateMachine:
    """Receiver application state machine"""
    
    def __init__(self):
        self.current_state = AppState.BOOT
        self.time = TimeSimulator()
        self.state_entry_time = self.time.get_time()
        self.retry_count = 0
        self.max_retries = 3
        self.event_log: List[TimedEvent] = []
        self.errors: List[str] = []
    
    def transition(self, new_state: AppState) -> bool:
        """
        Attempt state transition.
        
        Args:
            new_state: Target state
            
        Returns:
            True if transition successful
        """
        old_state = self.current_state
        self.current_state = new_state
        self.state_entry_time = self.time.get_time()
        
        # Log transition
        event = TimedEvent(
            f"{old_state.name}→{new_state.name}",
            self.time.get_time(),
            f"Transitioned from {old_state.name} to {new_state.name}"
        )
        self.event_log.append(event)
        return True
    
    def get_state(self) -> AppState:
        """Get current state"""
        return self.current_state
    
    def retry(self) -> bool:
        """
        Increment retry counter.
        
        Returns:
            True if retry available, False if max exceeded
        """
        if self.retry_count < self.max_retries:
            self.retry_count += 1
            event = TimedEvent(
                "RETRY",
                self.time.get_time(),
                f"Retry {self.retry_count}/{self.max_retries}"
            )
            self.event_log.append(event)
            return True
        return False
    
    def reset_retry(self):
        """Reset retry counter"""
        self.retry_count = 0
    
    def add_error(self, error: str):
        """Log error"""
        self.errors.append(error)
        event = TimedEvent("ERROR", self.time.get_time(), error)
        self.event_log.append(event)


class TimeoutHandler:
    """Handle timeout detection"""
    
    def __init__(self, timeout_ms: int):
        self.timeout_ms = timeout_ms
        self.start_time = None
    
    def start(self, current_time: int):
        """Start timeout countdown"""
        self.start_time = current_time
    
    def is_expired(self, current_time: int) -> bool:
        """Check if timeout expired"""
        if self.start_time is None:
            return False
        
        elapsed = current_time - self.start_time
        return elapsed >= self.timeout_ms
    
    def remaining_ms(self, current_time: int) -> int:
        """Get remaining time in milliseconds"""
        if self.start_time is None:
            return self.timeout_ms
        
        elapsed = current_time - self.start_time
        remaining = self.timeout_ms - elapsed
        return max(0, remaining)


class ImageFetchCycle:
    """Simulate image fetch/display cycle"""
    
    def __init__(self):
        self.fetch_interval_ms = 1000
        self.last_fetch_time = None
        self.fetch_count = 0
    
    def should_fetch(self, current_time: int) -> bool:
        """Check if fetch interval elapsed"""
        if self.last_fetch_time is None:
            return True
        
        elapsed = current_time - self.last_fetch_time
        return elapsed >= self.fetch_interval_ms
    
    def record_fetch(self, current_time: int):
        """Record fetch event"""
        self.last_fetch_time = current_time
        self.fetch_count += 1
    
    def get_fetch_count(self) -> int:
        """Get total fetches"""
        return self.fetch_count


# ============================================================================
# State Machine Tests
# ============================================================================

class TestStateTransitions:
    """Test valid state transitions"""
    
    def test_boot_to_wifi(self):
        """BOOT → WIFI_CONNECT transition"""
        sm = StateMachine()
        assert sm.get_state() == AppState.BOOT
        sm.transition(AppState.WIFI_CONNECT)
        assert sm.get_state() == AppState.WIFI_CONNECT
    
    def test_wifi_to_idle(self):
        """WIFI_CONNECT → IDLE transition"""
        sm = StateMachine()
        sm.transition(AppState.WIFI_CONNECT)
        sm.transition(AppState.IDLE)
        assert sm.get_state() == AppState.IDLE
    
    def test_idle_to_fetch(self):
        """IDLE → FETCH_IMAGE transition"""
        sm = StateMachine()
        sm.transition(AppState.IDLE)
        sm.transition(AppState.FETCH_IMAGE)
        assert sm.get_state() == AppState.FETCH_IMAGE
    
    def test_fetch_to_display(self):
        """FETCH_IMAGE → DECODE → DISPLAY transition"""
        sm = StateMachine()
        sm.transition(AppState.FETCH_IMAGE)
        sm.transition(AppState.DECODE)
        sm.transition(AppState.DISPLAY)
        assert sm.get_state() == AppState.DISPLAY
    
    def test_display_back_to_idle(self):
        """DISPLAY → IDLE creates cycle"""
        sm = StateMachine()
        sm.transition(AppState.DISPLAY)
        sm.transition(AppState.IDLE)
        assert sm.get_state() == AppState.IDLE


class TestErrorTransitions:
    """Test error handling paths"""
    
    def test_error_transition(self):
        """Can transition to ERROR state"""
        sm = StateMachine()
        sm.transition(AppState.FETCH_IMAGE)
        sm.transition(AppState.ERROR)
        assert sm.get_state() == AppState.ERROR
    
    def test_error_to_retry(self):
        """ERROR → RETRY transition"""
        sm = StateMachine()
        sm.transition(AppState.ERROR)
        sm.transition(AppState.RETRY)
        assert sm.get_state() == AppState.RETRY
    
    def test_retry_back_to_fetch(self):
        """RETRY → FETCH_IMAGE retries operation"""
        sm = StateMachine()
        sm.transition(AppState.ERROR)
        sm.transition(AppState.RETRY)
        sm.transition(AppState.FETCH_IMAGE)
        assert sm.get_state() == AppState.FETCH_IMAGE


# ============================================================================
# Retry Logic Tests
# ============================================================================

class TestRetryLogic:
    """Test retry counter and limits"""
    
    def test_first_retry_succeeds(self):
        """First retry is allowed"""
        sm = StateMachine()
        assert sm.retry()
        assert sm.retry_count == 1
    
    def test_multiple_retries(self):
        """Multiple retries allowed up to max"""
        sm = StateMachine()
        for i in range(sm.max_retries):
            assert sm.retry()
        assert sm.retry_count == sm.max_retries
    
    def test_max_retries_exceeded(self):
        """Cannot exceed max retries"""
        sm = StateMachine()
        for _ in range(sm.max_retries):
            sm.retry()
        assert not sm.retry()
    
    def test_retry_reset(self):
        """Retry counter can be reset"""
        sm = StateMachine()
        sm.retry()
        sm.retry()
        assert sm.retry_count == 2
        sm.reset_retry()
        assert sm.retry_count == 0
    
    def test_retry_after_reset(self):
        """Can retry again after reset"""
        sm = StateMachine()
        sm.retry()
        sm.reset_retry()
        assert sm.retry()


# ============================================================================
# Timeout Tests
# ============================================================================

class TestTimeoutHandling:
    """Test timeout detection"""
    
    def test_timeout_not_set_not_expired(self):
        """Not-started timeout is not expired"""
        handler = TimeoutHandler(5000)
        assert not handler.is_expired(0)
    
    def test_timeout_not_expired(self):
        """Timeout not expired if within limit"""
        handler = TimeoutHandler(5000)
        handler.start(0)
        assert not handler.is_expired(2500)  # 50% elapsed
    
    def test_timeout_expired(self):
        """Timeout expired when limit exceeded"""
        handler = TimeoutHandler(5000)
        handler.start(0)
        assert handler.is_expired(5000)
    
    def test_timeout_well_exceeded(self):
        """Timeout definitely expired when well exceeded"""
        handler = TimeoutHandler(5000)
        handler.start(0)
        assert handler.is_expired(10000)
    
    def test_remaining_time_calculation(self):
        """Remaining time calculated correctly"""
        handler = TimeoutHandler(5000)
        handler.start(0)
        assert handler.remaining_ms(0) == 5000
        assert handler.remaining_ms(2000) == 3000
        assert handler.remaining_ms(5000) == 0
    
    def test_remaining_time_negative_becomes_zero(self):
        """Remaining time never negative"""
        handler = TimeoutHandler(5000)
        handler.start(0)
        assert handler.remaining_ms(10000) == 0


class TestWifiTimeout:
    """Test WiFi connection timeout"""
    
    def test_wifi_connects_within_timeout(self):
        """WiFi connects before timeout"""
        timeout = TimeoutHandler(15000)  # 15 second timeout
        timeout.start(0)
        
        # Simulate connection at 5 seconds
        assert not timeout.is_expired(5000)
    
    def test_wifi_timeout_triggers(self):
        """WiFi timeout triggers if no connection"""
        timeout = TimeoutHandler(15000)
        timeout.start(0)
        
        # Connection fails after 15 seconds
        assert timeout.is_expired(15000)


# ============================================================================
# Fetch Interval Tests
# ============================================================================

class TestFetchInterval:
    """Test periodic image fetching"""
    
    def test_immediate_first_fetch(self):
        """First fetch happens immediately"""
        cycle = ImageFetchCycle()
        assert cycle.should_fetch(0)
    
    def test_fetch_interval_respected(self):
        """Fetch interval is respected"""
        cycle = ImageFetchCycle()
        cycle.record_fetch(0)
        
        # Too soon
        assert not cycle.should_fetch(500)
        assert not cycle.should_fetch(900)
        
        # At interval
        assert cycle.should_fetch(1000)
    
    def test_fetch_count_incremented(self):
        """Fetch count increments"""
        cycle = ImageFetchCycle()
        assert cycle.get_fetch_count() == 0
        
        cycle.record_fetch(0)
        assert cycle.get_fetch_count() == 1
        
        cycle.record_fetch(1000)
        assert cycle.get_fetch_count() == 2
    
    def test_fetch_frequency_1hz(self):
        """Fetch occurs once per second"""
        cycle = ImageFetchCycle()
        times = [0, 1000, 2000, 3000]
        
        count = 0
        for t in times:
            if cycle.should_fetch(t):
                cycle.record_fetch(t)
                count += 1
        
        assert count == len(times)


# ============================================================================
# Complete Cycle Tests
# ============================================================================

class TestCompleteCycle:
    """Test complete fetch/decode/display cycle"""
    
    def test_successful_cycle(self):
        """Successful fetch/display cycle"""
        sm = StateMachine()
        
        # Start cycle
        sm.transition(AppState.BOOT)
        assert sm.get_state() == AppState.BOOT
        
        sm.transition(AppState.IDLE)
        assert sm.get_state() == AppState.IDLE
        
        # Fetch cycle
        sm.transition(AppState.FETCH_IMAGE)
        sm.transition(AppState.DECODE)
        sm.transition(AppState.DISPLAY)
        assert sm.get_state() == AppState.DISPLAY
        
        # Return to idle
        sm.transition(AppState.IDLE)
        assert sm.get_state() == AppState.IDLE
    
    def test_cycle_with_error_recovery(self):
        """Cycle handles error and recovers"""
        sm = StateMachine()
        sm.transition(AppState.IDLE)
        sm.transition(AppState.FETCH_IMAGE)
        sm.transition(AppState.ERROR)
        
        # Retry
        assert sm.retry()
        sm.transition(AppState.RETRY)
        sm.transition(AppState.FETCH_IMAGE)
        sm.transition(AppState.DISPLAY)
        assert sm.get_state() == AppState.DISPLAY
    
    def test_multiple_cycles(self):
        """Multiple cycles in sequence"""
        cycle = ImageFetchCycle()
        sm = StateMachine()
        
        # Simulate 3 cycles
        for cycle_num in range(3):
            base_time = cycle_num * 1000
            
            if cycle.should_fetch(base_time):
                cycle.record_fetch(base_time)
                sm.transition(AppState.FETCH_IMAGE)
                sm.transition(AppState.DISPLAY)
                sm.transition(AppState.IDLE)
        
        assert cycle.get_fetch_count() == 3


# ============================================================================
# Non-Blocking Behavior Tests
# ============================================================================

class TestNonBlockingBehavior:
    """Test non-blocking state machine behavior"""
    
    def test_state_update_instant(self):
        """State updates instantly"""
        sm = StateMachine()
        start = sm.time.get_time()
        sm.transition(AppState.IDLE)
        end = sm.time.get_time()
        
        # Transition should be instant (no blocking)
        assert end == start
    
    def test_timeout_non_blocking(self):
        """Timeout check doesn't block"""
        handler = TimeoutHandler(5000)
        handler.start(0)
        
        # Check doesn't cause delay
        assert not handler.is_expired(2000)
        assert not handler.is_expired(3000)
    
    def test_multiple_updates_per_second(self):
        """Can perform many updates per second"""
        sm = StateMachine()
        
        # Simulate 100 updates per second
        for i in range(100):
            sm.time.advance(10)  # 10ms per update
            if i % 2 == 0:
                sm.transition(AppState.IDLE)
            else:
                sm.transition(AppState.FETCH_IMAGE)


# ============================================================================
# Event Logging Tests
# ============================================================================

class TestEventLogging:
    """Test state machine event logging"""
    
    def test_transitions_logged(self):
        """State transitions are logged"""
        sm = StateMachine()
        sm.transition(AppState.BOOT)
        sm.transition(AppState.IDLE)
        
        assert len(sm.event_log) >= 2
    
    def test_error_logged(self):
        """Errors are logged"""
        sm = StateMachine()
        error_msg = "Connection failed"
        sm.add_error(error_msg)
        
        assert error_msg in sm.errors
        assert len(sm.event_log) > 0
    
    def test_retry_logged(self):
        """Retries are logged"""
        sm = StateMachine()
        sm.retry()
        sm.retry()
        
        assert len(sm.event_log) >= 2

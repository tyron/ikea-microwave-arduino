# Testing Guide for IKEA Microwave Arduino

This project includes comprehensive unit tests for the timer logic. The tests can be run on your local machine without needing the Arduino hardware.

## Test Coverage

The unit tests cover the following components:

### Timer Logic (`include/timer_logic.h`)
- **parseTimerInput()**: Converts user input string to minutes/seconds
  - Handles various input formats (1-4 digits)
  - Normalizes seconds over 59 (e.g., 1:75 → 2:15)
  - Validates input and returns error for invalid inputs

- **decrementTimer()**: Countdown logic
  - Decrements seconds correctly
  - Handles minute rollover (2:00 → 1:59)
  - Detects when timer reaches zero

- **nonBlockingDelay()**: Non-blocking timing function
  - Returns true when interval has elapsed
  - Does not block program execution
  - Correctly tracks time using millis()

## Running Tests

### Method 1: Using PlatformIO (Recommended)

If you have PlatformIO installed with internet access:

```bash
# Install PlatformIO
pip install platformio

# Run tests
pio test -e native
```

### Method 2: Manual Compilation with g++

If PlatformIO platform installation fails, you can compile and run tests manually:

```bash
# 1. Clone Unity testing framework
cd /tmp
git clone --depth 1 https://github.com/ThrowTheSwitch/Unity.git

# 2. Compile the tests
cd /home/user/ikea-microwave-arduino
g++ -std=c++11 -DUNIT_TEST \
    -I./include \
    -I/tmp/Unity/src \
    test/test_timer_logic/test_timer_logic.cpp \
    /tmp/Unity/src/unity.c \
    -o test_runner

# 3. Run the tests
./test_runner
```

### Expected Output

When all tests pass, you should see:

```
test/test_timer_logic/test_timer_logic.cpp:221:test_parseTimerInput_simple_case:PASS
test/test_timer_logic/test_timer_logic.cpp:222:test_parseTimerInput_four_digits:PASS
...
-----------------------
19 Tests 0 Failures 0 Ignored
OK
```

## Test Files

- `include/timer_logic.h` - Testable timer logic functions extracted from main.cpp
- `test/test_timer_logic/test_timer_logic.cpp` - 19 comprehensive unit tests

## Hardware Testing

For full integration testing with actual hardware:

```bash
# Build and upload to Arduino
pio run --target upload

# Monitor serial output
pio device monitor
```

The code outputs debug information via Serial at 9600 baud:
- Key presses
- Timer countdown values
- State transitions

## Adding New Tests

To add new tests:

1. Open `test/test_timer_logic/test_timer_logic.cpp`
2. Add your test function following the pattern:
   ```cpp
   void test_my_new_feature(void) {
       // Arrange
       // Act
       // Assert
       TEST_ASSERT_EQUAL_INT(expected, actual);
   }
   ```
3. Register the test in `main()`:
   ```cpp
   RUN_TEST(test_my_new_feature);
   ```
4. Recompile and run

## CI/CD Integration

You can integrate these tests into your CI/CD pipeline:

```yaml
# Example GitHub Actions workflow
- name: Run Unit Tests
  run: |
    pip install platformio
    pio test -e native
```

## Test Results

Current test status: **All 19 tests passing** ✓

- parseTimerInput: 10 tests
- decrementTimer: 5 tests
- nonBlockingDelay: 4 tests

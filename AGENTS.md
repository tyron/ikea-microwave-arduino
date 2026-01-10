# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is an Arduino-based microwave timer controller built with PlatformIO. The system uses a state machine to manage timer input, countdown, and completion states. It features:
- **4x3 matrix keypad** for user input (digits 0-9, * for cancel, # for start)
- **TM1637 4-digit 7-segment display** (shows time in MM:SS format during countdown, HH:MM from RTC otherwise)
- **DS3231 RTC module** for displaying current time when idle
- **State machine architecture** with 5 states (SHOW_CURRENT_TIME, WAITING_INPUT, SETTING_TIMER, COUNTDOWN, COMPLETE)

## Hardware Pins

- Display: CLK=11, DIO=10
- Keypad rows: 9, 8, 7, 6
- Keypad cols: 5, 4, 3
- RTC: I2C at 0x68

## Build and Upload Commands

```bash
# Build the project
pio run

# Upload to Arduino Uno
pio run --target upload

# Build and upload in one command
pio run -t upload

# Open serial monitor (9600 baud)
pio device monitor

# Clean build artifacts
pio run --target clean
```

## Architecture

### State Machine Flow

1. **SHOW_CURRENT_TIME**: Default state - displays current time from RTC with blinking colon
2. **WAITING_INPUT**: Shows only colon, waiting for digit entry
3. **SETTING_TIMER**: User entering 1-4 digits (MM:SS format, e.g., "130" → "1:30")
4. **COUNTDOWN**: Timer actively counting down every second (non-blocking)
5. **COMPLETE**: Timer finished - blinks "00:00" 4 times, then returns to SHOW_CURRENT_TIME

### Key Implementation Details

- **Non-blocking countdown**: Uses `nonBlockingDelay()` helper to tick every second without blocking `loop()`
- **Input timeout**: Automatically resets to SHOW_CURRENT_TIME if no keypress for 60 seconds during SETTING_TIMER
- **Input parsing**: Handles leading zeros correctly (e.g., "001" displays as "0:01", not "  :1")
- **Input normalization**: Automatically converts seconds > 59 (e.g., "199" becomes 2:39)
- **Cancel anytime**: Press `*` to cancel and return to showing current time
- **RTC initialization**: Uncomment and modify `rtc.set()` in `setup()` to set initial date/time in UTC, then re-comment
- **Automatic DST handling**: Clock automatically switches between EST (UTC-5) and EDT (UTC-4) based on US DST rules

### Display Functions

- `updateInputDisplay()`: Shows partial input with proper formatting and leading zeros during SETTING_TIMER
- `updateDisplay()`: Shows MM:SS format during COUNTDOWN with leading zeros
- `displayCurrentTime()`: Shows HH:MM from RTC with blinking colon based on seconds

### Timezone & DST Functions

- `getNthWeekdayOfMonth()`: Calculates which day of the month a specific weekday occurrence falls on (e.g., "2nd Sunday of March"). Uses Zeller's congruence algorithm for day-of-week calculations.
- `isDST()`: Determines if DST is active based on US rules (2nd Sunday in March at 2:00 AM → 1st Sunday in November at 2:00 AM)
- `applyEasternTimeOffset()`: Applies the correct UTC offset (-4 for EDT, -5 for EST) to convert UTC time to local Eastern time, handles day rollover
- **RTC must be set to UTC time** for correct display. The code automatically applies EST/EDT offset based on the date.

## Dependencies

All managed via PlatformIO (see [platformio.ini](platformio.ini)):
- `chris--a/Keypad@^3.1.1`
- `TM1637Display` (from GitHub)
- `naguissa/uRTCLib@^6.9.6`

## Development Notes

- The entire application logic is in [src/main.cpp](src/main.cpp)
- State transitions happen in `handleInput()`, state execution in `executeState()`
- Serial output at 9600 baud logs key presses and countdown ticks for debugging
- COMPLETE state uses blocking `delay()` for blink animation (acceptable as it's terminal state)

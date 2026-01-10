# AGENTS.md

This file provides guidance to AI agents (and human developers) when working with code in this repository.

## Project Overview

This is an Arduino-based microwave timer controller built with PlatformIO. The system uses a state machine to manage timer input, countdown, and completion states. It features:
- **4x3 matrix keypad** for user input (digits 0-9, * for cancel, # for start)
- **TM1637 4-digit 7-segment display** (shows time in MM:SS format during countdown, HH:MM from RTC otherwise)
- **DS3231 RTC module** for displaying current time when idle
- **NeoPixel Ring (24 LED)** for visual feedback
- **State machine architecture** with 5 states

## Hardware Pins

- **TM1637 Display**: CLK=9, DIO=10
- **Keypad Rows**: 3, 8, 7, 5
- **Keypad Cols**: 4, 2, 6
- **Buzzer**: 11
- **NeoPixel Data**: 13
- **RTC**: I2C at 0x68 (SDA/SCL pins depend on board, usually A4/A5 on Uno)

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

The system operates in one of the following 5 states:

1. **SHOW_CURRENT_TIME**: 
   - **Behavior**: Displays current time (HH:MM) from RTC with blinking colon. NeoPixel ring shows standby color (Yellow).
   - **Transitions**: 
     - To `SHOW_CURRENT_TIME_NO_LED` after 15 seconds of inactivity.
     - To `SETTING_TIMER` if a digit (0-9) is pressed.
     - To `SHOW_CURRENT_TIME` (reset) if `*` is pressed.

2. **SHOW_CURRENT_TIME_NO_LED**: 
   - **Behavior**: Same as `SHOW_CURRENT_TIME` but NeoPixel ring is OFF.
   - **Transitions**:
     - To `SETTING_TIMER` if a digit (0-9) is pressed.
     - To `SHOW_CURRENT_TIME` if `*` is pressed.

3. **SETTING_TIMER**: 
   - **Behavior**: User enters 1-4 digits. Display updates as "M:SS" or "MM:SS".
   - **Transitions**:
     - To `COUNTDOWN` if `#` is pressed and input > 0.
     - To `SHOW_CURRENT_TIME` if `*` is pressed (Cancel).
     - To `SHOW_CURRENT_TIME` if no input for 60 seconds (Timeout).

4. **COUNTDOWN**: 
   - **Behavior**: Timer actively counts down. Display shows "MM:SS". NeoPixel ring animates (rotating pattern).
   - **Transitions**:
     - To `COMPLETE` when timer reaches 00:00.
     - To `SHOW_CURRENT_TIME_NO_LED` if `*` is pressed (Cancel).

5. **COMPLETE**: 
   - **Behavior**: Timer finished. Display blinks "00:00" and buzzer beeps 4 times. NeoPixel ring shows complete color (Yellow).
   - **Transitions**:
     - To `SHOW_CURRENT_TIME` after blink sequence finishes (approx 4 cycles).
     - To `SHOW_CURRENT_TIME` immediately if any key is pressed.

### Key Implementation Details

- **Non-blocking Execution**: Uses `nonBlockingDelay()` helper to manage timing for countdowns, animations, and timeouts without blocking `loop()`. Even the `COMPLETE` state blinking/beeping is non-blocking.
- **Input Parsing**: Handles leading zeros correctly (e.g., "001" displays as "0:01").
- **Input Normalization**: Automatically converts seconds > 59 (e.g., input "199" becomes 2:39).
- **RTC & DST**: 
  - `rtc.set()` in `setup()` can be used to set UTC time.
  - `applyEasternTimeOffset()` automatically calculates EST/EDT based on US DST rules (2nd Sun March - 1st Sun Nov) to display local time.
  - `getNthWeekdayOfMonth()` calculates dynamic DST transition dates.

## Dependencies

Managed via PlatformIO:
- `Keypad`
- `TM1637Display`
- `uRTCLib` (for DS3231)
- `Adafruit NeoPixel`

## Development Notes

- **Main Logic**: Located in `src/main.cpp`.
- **State Handling**: `handleInput()` manages transitions; `executeState()` manages active behavior.
- **Visuals**: `Rotator` class manages NeoPixel animations.
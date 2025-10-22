#include <Arduino.h>

#include <Keypad.h>
#include <TM1637Display.h>

// https://lastminuteengineers.com/ds3231-rtc-arduino-tutorial/
#include "uRTCLib.h"

// constants for 7-segment display
#define CLK 11
#define DIO 10

// Create a display object of type TM1637Display
TM1637Display display = TM1637Display(CLK, DIO);

// Create an array that turns all segments ON
const uint8_t allON[] = { 0xff, 0xff, 0xff, 0xff };

// Create an array that displays only the collon
const uint8_t onlyCollon[] = { 0x00, SEG_DP, 0x00, 0x00 };

// constants for keypad
const byte ROWS = 4; //four rows
const byte COLS = 3; //three columns
char keys[ROWS][COLS] = {
  {'1','2','3'},
  {'4','5','6'},
  {'7','8','9'},
  {'*','0','#'}
};
byte rowPins[ROWS] = {9, 8, 7, 6}; //connect to the row pinouts of the keypad
byte colPins[COLS] = {5, 4, 3}; //connect to the column pinouts of the keypad

Keypad keypad = Keypad( makeKeymap(keys), rowPins, colPins, ROWS, COLS );

String inputNumber = ""; // Variable to store the number sequence

// DS3231 RTC object
uRTCLib rtc(0x68);

// State machine definitions (mirrors pattern used in IKEA_DUKTIG_clock.ino)
#define ShowCurrentTime   1   // current time display
#define SettingTimer   2   // user inputting timer
#define NumberKeyPress   7   // user inputting timer
#define ResetTimer 3   // just collon on
#define StartTimer 4
#define CountDown  5
#define Beeping    6

int intState = ShowCurrentTime; // current state
int timerMinutes = 0;    // countdown minutes
int timerSeconds = 0;    // countdown seconds

bool nonBlockingDelay(unsigned long &previousMillis, unsigned long interval) {
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    return true;
  }
  return false;
}

// Function to update the display based on the current input
// Handles formatting and colon placement including cases where input has leading zeros
// eg. "001" is shown as "0:01" (the default function would either display as "  :1" with no leading zeros or "00:01" with leading zeros)
// Function required only during input phase as leading zeros for 4-digit are enforced in the countdown phase
void updateInputDisplay(const String &input) {
  uint8_t segments[4] = {0x00, 0x00, 0x00, 0x00}; // Initialize all segments to blank

  int length = input.length();
  if (length > 0 && length <= 4) {
    for (int i = 0; i < length; i++) {
      segments[4 - length + i] = display.encodeDigit(input[i] - '0'); // Encode digits
    }
    segments[1] |= SEG_DP; // Add colon at the middle
  }
  display.setSegments(segments);
}

// Function to update the display in MM:SS format
void updateDisplay(int minutes, int seconds) {
  // concatenate minutes and seconds for display
  int displayTime = minutes * 100 + seconds;
  display.showNumberDecEx(displayTime, 0b01000000, true, 4, 0);
}

// Function to update the display in HH:MM format, blinking colon based on seconds
void displayCurrentTime(int hours, int minutes, int seconds) {
  // concatenate hours and minutes for display
  int displayTime = hours * 100 + minutes;

  // Define the bitmask for the colon. 0b01000000 is for the colon between digits 1 and 2.
  // To make it flash, you can toggle this value every second.
  uint8_t colonMask = (seconds % 2 == 0) ? 0x40 : 0;

  display.showNumberDecEx(displayTime, colonMask, true, 4, 0);
}

void setup(){
  Serial.begin(9600);

  // Set the brightness to 5 (0=dimmest 7=brightest)
  display.setBrightness(0);

  // prints 00:00 at start
  // display.showNumberDecEx(0, 0b01000000, true, 4, 0);

  // Set all segments ON
  display.setSegments(allON);
  delay(100);
  display.setSegments(onlyCollon);

  URTCLIB_WIRE.begin();

  // Start in show time state
  intState = ShowCurrentTime;

  // Comment out below line once you set the date & time.
  // Following line sets the RTC with an explicit date & time
  // for example to set April 14 2025 at 12:56 you would call:
  // rtc.set(0, 20, 14, 4, 22, 10, 25);
  // rtc.set(second, minute, hour, dayOfWeek, dayOfMonth, month, year)
  // set day of week (1=Sunday, 7=Saturday)



}

void loop(){
  static unsigned long lastKeyPressMillis = 0;    // Track time of the last key press
  static unsigned long countdownTickMillis = 0;   // Track countdown ticks

  // --- Phase 1: Read inputs and update/transition state ---
  char key = keypad.getKey();
  if (key) {
    lastKeyPressMillis = millis(); // Update the last key press time

    switch (intState) {
      case ShowCurrentTime:
      case SettingTimer:
        Serial.println("Key Pressed: " + String(key));
        if (key >= '0' && key <= '9') {
          intState = NumberKeyPress;
        }
        if (key == '#') {
          intState = StartTimer;
        }
        if (key == '*') {
          intState = ResetTimer;
        }
        break;

      case CountDown:
        if (key == '*') {
          intState = ResetTimer;
        }
        break;
      case Beeping:
        break;
    }
  }

  // --- Phase 2: State machine actions ---
  // Serial.println("Current State: " + String(intState));
  switch (intState) {
    case ShowCurrentTime: {
      rtc.refresh();
      displayCurrentTime(rtc.hour(), rtc.minute(), rtc.second());
      break;
    }

    case NumberKeyPress: {
      if (inputNumber.length() >= 4) {
        Serial.println("Max 4 digits reached.");
      } else {
        inputNumber += key;
        Serial.println(key);
      }
      updateInputDisplay(inputNumber);

      intState = SettingTimer;

      break;
    }

    case SettingTimer: {
      // Reset display to current time if no key press for 60 seconds
      if (nonBlockingDelay(lastKeyPressMillis, 5000)) {
        Serial.println("No key press for 5 seconds, resetting display.");
        intState = ResetTimer;
      }
      break;
    }

    case StartTimer: {
      // Start countdown when user confirms input
      if (inputNumber.length() > 0) {
        int totalSeconds = inputNumber.toInt();
        timerMinutes = totalSeconds / 100;
        timerSeconds = totalSeconds % 100;
        // Normalize seconds > 59
        timerMinutes += timerSeconds / 60;
        timerSeconds = timerSeconds % 60;

        // Move to CountDown state
        intState = CountDown;

        // show initial time on display
        updateDisplay(timerMinutes, timerSeconds);

        // reset countdown tick timer
        countdownTickMillis = millis();
      }
      inputNumber = ""; // clear input buffer regardless
      break;
    }

    case ResetTimer: {
      // Show only collon
      display.setSegments(onlyCollon);
      // Reset timers
      timerMinutes = 0;
      timerSeconds = 0;
      inputNumber = "";

      // Move to ShowTime state
      intState = ShowCurrentTime;
      break;
    }

    case CountDown: {
      // Tick countdown every second (non-blocking)
      if (nonBlockingDelay(countdownTickMillis, 1000)) {
        if (timerSeconds == 0) {
          if (timerMinutes > 0) {
            timerMinutes--;
            timerSeconds = 59;
          }
        } else {
          timerSeconds--;
        }
        if (timerMinutes < 10) Serial.print("0");
        Serial.print(timerMinutes);
        Serial.print(":");
        if (timerSeconds < 10) Serial.print("0");
        Serial.println(timerSeconds);
        updateDisplay(timerMinutes, timerSeconds);
      }

      // Allow '*' to interrupt is handled in Phase 1 input processing above

      // When countdown reaches zero, transition to Beeping
      if (timerMinutes == 0 && timerSeconds == 0) {
        intState = Beeping;
      }
      break;
    }

    case Beeping: {
      // Blink 00:00 three times
      int blinkCount = 0;
      while (blinkCount < 4) {
        display.showNumberDecEx(0, 0b01000000, true, 4, 0); // Show 00:00
        delay(500);
        display.setSegments(onlyCollon); // Clear display
        delay(200);
        blinkCount++;
      }
      // After beeping go back to ShowTime
      intState = ShowCurrentTime;
      break;
    }
  }

}

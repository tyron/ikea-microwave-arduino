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

char daysOfTheWeek[7][12] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };


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
void updateDisplay(int minutes, int seconds, bool blinkColon = false) {
  // concatenate minutes and seconds for display
  int displayTime = minutes * 100 + seconds;

  // Define the bitmask for the colon. 0b01000000 is for the colon between digits 1 and 2.
  // To make it flash, you can toggle this value every second.
  uint8_t colonMask = (!blinkColon || (seconds % 2 == 0)) ? 0x40 : 0;

  display.showNumberDecEx(displayTime, colonMask, true, 4, 0);
}

void printDateTime() {
  rtc.refresh();
  Serial.print("Date: ");
  Serial.print(rtc.day());
  Serial.print("/");
  Serial.print(rtc.month());
  Serial.print("/");
  Serial.print(2000 + rtc.year()); // Assuming 21st century
  Serial.print(" ");
  Serial.print(daysOfTheWeek[rtc.dayOfWeek() - 1]);
  Serial.print(" Time: ");
  if (rtc.hour() < 10) Serial.print("0");
  Serial.print(rtc.hour());
  Serial.print(":");
  if (rtc.minute() < 10) Serial.print("0");
  Serial.print(rtc.minute());
  Serial.print(":");
  if (rtc.second() < 10) Serial.print("0");
  Serial.println(rtc.second());
}

void setup(){
  Serial.begin(9600);

  // Set the brightness to 5 (0=dimmest 7=brightest)
  display.setBrightness(0);

  // prints 00:00 at start
  // display.showNumberDecEx(0, 0b01000000, true, 4, 0);

  // Set all segments ON
  display.setSegments(allON);
  delay(1000);
  display.setSegments(onlyCollon);

  URTCLIB_WIRE.begin();

  // Comment out below line once you set the date & time.
  // Following line sets the RTC with an explicit date & time
  // for example to set April 14 2025 at 12:56 you would call:
  // rtc.set(0, 20, 14, 4, 22, 10, 25);
  // rtc.set(second, minute, hour, dayOfWeek, dayOfMonth, month, year)
  // set day of week (1=Sunday, 7=Saturday)



}

void loop(){
  static unsigned long previousDateTimeMillis = 0; // Track time for printDateTime
  static unsigned long lastKeyPressMillis = 0;    // Track time of the last key press

  char key = keypad.getKey();

  if (key) {
    lastKeyPressMillis = millis(); // Update the last key press time

    if (key >= '0' && key <= '9') { // If the key is a number
      if (inputNumber.length() >= 4) {
        Serial.println("Max 4 digits reached.");
      } else {
        inputNumber += key; // Append the number to the input string
        Serial.println(key);
      }
      updateInputDisplay(inputNumber); // Update display with formatted input
    } else if (key == '#') { // If the key is '#'
      if (inputNumber.length() > 0) { // Ensure there is a number to count down from
        int totalSeconds = inputNumber.toInt(); // Convert the string to total seconds
        int minutes = totalSeconds / 100; // Extract minutes
        int seconds = totalSeconds % 100; // Extract seconds

        // Adjust seconds if they exceed 59
        minutes += seconds / 60;
        seconds = seconds % 60;

        // show initial time on display
        updateDisplay(minutes, seconds);

        unsigned long previousMillis = millis(); // Store the current time
        while (minutes > 0 || seconds > 0) {
          if (nonBlockingDelay(previousMillis, 1000)) { // Check if 1 second has passed
            if (seconds == 0) {
              if (minutes > 0) {
                minutes--;
                seconds = 59;
              }
            } else {
              seconds--;
            }
            if (minutes < 10) {
              Serial.print("0"); // Add leading zero for single-digit minutes
            }
            Serial.print(minutes);
            Serial.print(":");
            if (seconds < 10) {
              Serial.print("0"); // Add leading zero for single-digit seconds
            }
            Serial.println(seconds);
            updateDisplay(minutes, seconds);
          }

          char interruptKey = keypad.getKey(); // Check for interrupt key
          if (interruptKey == '*') { // If '*' is pressed, stop the countdown
            Serial.println("Countdown stopped.");
            display.setSegments(onlyCollon);
            break;
          }
        }

        // Blink 00:00 three times when countdown reaches zero
        if (minutes == 0 && seconds == 0) {
          int blinkCount = 0;
          while (blinkCount < 4) {
            display.showNumberDecEx(0, 0b01000000, true, 4, 0); // Show 00:00
            delay(500);
            display.setSegments(onlyCollon); // Clear display
            delay(200);
            blinkCount++;
          }
        }
      }
      inputNumber = ""; // Reset the input number after countdown
    } else if (key == '*') { // If the key is '*', clear the input
      inputNumber = "";
      Serial.println("Input cleared.");
      display.setSegments(onlyCollon);
    }
  }

  // Reset display to current time if no key press for 60 seconds when dirty,
  // or if it was empty for 5 seconds
  if (nonBlockingDelay(lastKeyPressMillis, 60000) || (inputNumber == "" && nonBlockingDelay(previousDateTimeMillis, 5000))) {
    Serial.println("No key press for 5 seconds, updating display.");
    rtc.refresh();
    updateDisplay(rtc.hour(), rtc.minute(), true);
  }
}

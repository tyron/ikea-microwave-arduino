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
const uint8_t allON[] = {0xff, 0xff, 0xff, 0xff};

// Create an array that displays only the collon
const uint8_t onlyCollon[] = {0x00, SEG_DP, 0x00, 0x00};

// constants for keypad
const byte ROWS = 4; // four rows
const byte COLS = 3; // three columns
char keys[ROWS][COLS] = {
    {'1', '2', '3'},
    {'4', '5', '6'},
    {'7', '8', '9'},
    {'*', '0', '#'}};
byte rowPins[ROWS] = {9, 8, 7, 6}; // connect to the row pinouts of the keypad
byte colPins[COLS] = {5, 4, 3};    // connect to the column pinouts of the keypad

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

String inputNumber = ""; // Variable to store the number sequence

// DS3231 RTC object
uRTCLib rtc(0x68);

// DST calculation helper functions
// Calculates the day of month for the Nth occurrence of a weekday
// weekday: 1=Sunday, 7=Saturday
int getNthWeekdayOfMonth(int year, int month, int weekday, int n) {
  // Find the first day of the month's day of week
  // Using Zeller's congruence for day of week calculation
  int q = 1; // day of month (1st)
  int m = month;
  int y = year;

  if (m < 3) {
    m += 12;
    y--;
  }

  int k = y % 100;
  int j = y / 100;
  int h = (q + ((13 * (m + 1)) / 5) + k + (k / 4) + (j / 4) - (2 * j)) % 7;

  // Convert Zeller's result (0=Saturday) to our format (1=Sunday)
  int firstDayOfWeek = ((h + 6) % 7) + 1;

  // Calculate which day of the month the nth occurrence falls on
  int offset = (weekday - firstDayOfWeek + 7) % 7;
  int nthDay = 1 + offset + (n - 1) * 7;

  return nthDay;
}

// Check if DST is active for Eastern Time
// DST: Second Sunday in March at 2:00 AM through first Sunday in November at 2:00 AM
bool isDST(int year, int month, int day, int hour) {
  // DST starts second Sunday in March
  int marchStart = getNthWeekdayOfMonth(year, 3, 1, 2); // 2nd Sunday of March

  // DST ends first Sunday in November
  int novemberEnd = getNthWeekdayOfMonth(year, 11, 1, 1); // 1st Sunday of November

  // Before March or after November - definitely EST
  if (month < 3 || month > 11) return false;
  if (month > 3 && month < 11) return true;

  // In March - check if we're past the transition
  if (month == 3) {
    if (day < marchStart) return false;
    if (day > marchStart) return true;
    // On transition day, check hour (2 AM transition)
    return hour >= 2;
  }

  // In November - check if we're before the transition
  if (month == 11) {
    if (day < novemberEnd) return true;
    if (day > novemberEnd) return false;
    // On transition day, check hour (2 AM transition)
    return hour < 2;
  }

  return false;
}

// Apply EST/EDT offset to UTC time stored in RTC
void applyEasternTimeOffset(int &hour, int &day, int month, int year) {
  // Determine offset: EST = UTC-5, EDT = UTC-4
  int offset = isDST(year, month, day, hour) ? -4 : -5;

  hour += offset;

  // Handle day rollover
  if (hour < 0) {
    hour += 24;
    day--;
    // Note: We don't handle month/year rollback as it's edge case for display only
  } else if (hour >= 24) {
    hour -= 24;
    day++;
    // Note: We don't handle month/year rollover as it's edge case for display only
  }
}

// State machine definitions
enum State {
  SHOW_CURRENT_TIME,    // Display current time from RTC
  WAITING_INPUT,        // Waiting for input (colon only)
  SETTING_TIMER,        // User is entering timer digits
  COUNTDOWN,            // Timer is actively counting down
  COMPLETE              // Timer finished, beeping/blinking
};

State currentState = SHOW_CURRENT_TIME;
int timerMinutes = 0;           // countdown minutes
int timerSeconds = 0;           // countdown seconds
unsigned long countdownTickMillis = 0; // Timer for countdown ticks

bool nonBlockingDelay(unsigned long &previousMillis, unsigned long interval)
{
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval)
  {
    previousMillis = currentMillis;
    return true;
  }
  return false;
}

// Function to update the display based on the current input
// Handles formatting and colon placement including cases where input has leading zeros
// eg. "001" is shown as "0:01" (the default function would either display as "  :1" with no leading zeros or "00:01" with leading zeros)
// Function required only during input phase as leading zeros for 4-digit are enforced in the countdown phase
void updateInputDisplay(const String &input)
{
  uint8_t segments[4] = {0x00, 0x00, 0x00, 0x00}; // Initialize all segments to blank

  int length = input.length();
  if (length > 0 && length <= 4)
  {
    for (int i = 0; i < length; i++)
    {
      segments[4 - length + i] = display.encodeDigit(input[i] - '0'); // Encode digits
    }
    segments[1] |= SEG_DP; // Add colon at the middle
  }
  display.setSegments(segments);
}

// Function to update the display in MM:SS format
void updateDisplay(int minutes, int seconds)
{
  // concatenate minutes and seconds for display
  int displayTime = minutes * 100 + seconds;
  display.showNumberDecEx(displayTime, 0b01000000, true, 4, 0);
}

// Function to update the display in HH:MM format, blinking colon based on seconds
void displayCurrentTime(int hours, int minutes, int seconds)
{
  // concatenate hours and minutes for display
  int displayTime = hours * 100 + minutes;

  // Define the bitmask for the colon. 0b01000000 is for the colon between digits 1 and 2.
  // To make it flash, you can toggle this value every second.
  uint8_t colonMask = (seconds % 2 == 0) ? 0x40 : 0;

  display.showNumberDecEx(displayTime, colonMask, true, 4, 0);
}

// Handle input and state transitions
void handleInput()
{
  static unsigned long lastKeyPressMillis = 0;
  char key = keypad.getKey();

  if (key)
  {
    lastKeyPressMillis = millis();
    Serial.println("Key Pressed: " + String(key));

    switch (currentState)
    {
    case SHOW_CURRENT_TIME:
    case WAITING_INPUT:
    case SETTING_TIMER:
      if (key >= '0' && key <= '9')
      {
        // Handle number input
        if (inputNumber.length() < 4)
        {
          inputNumber += key;
          currentState = SETTING_TIMER;
          updateInputDisplay(inputNumber);
        }
      }
      else if (key == '#')
      {
        // Start timer
        if (inputNumber.length() > 0)
        {
          // Parse input and start countdown
          int totalSeconds = inputNumber.toInt();
          timerMinutes = totalSeconds / 100;
          timerSeconds = totalSeconds % 100;
          // Normalize seconds > 59
          timerMinutes += timerSeconds / 60;
          timerSeconds = timerSeconds % 60;

          currentState = COUNTDOWN;
          inputNumber = "";
          // Display the initial time immediately
          updateDisplay(timerMinutes, timerSeconds);
          // Reset countdown tick timer to start fresh
          countdownTickMillis = millis();
        }
      }
      else if (key == '*')
      {
        // Reset/Cancel
        inputNumber = "";
        timerMinutes = 0;
        timerSeconds = 0;
        display.setSegments(onlyCollon);
        currentState = SHOW_CURRENT_TIME;
      }
      break;

    case COUNTDOWN:
      if (key == '*')
      {
        // Cancel countdown
        inputNumber = "";
        timerMinutes = 0;
        timerSeconds = 0;
        display.setSegments(onlyCollon);
        currentState = SHOW_CURRENT_TIME;
      }
      break;

    case COMPLETE:
      // Any key press exits complete state
      currentState = SHOW_CURRENT_TIME;
      break;
    }
  }

  // Timeout handling - reset if no input for 60 seconds while setting timer
  if (currentState == SETTING_TIMER)
  {
    if (millis() - lastKeyPressMillis >= 60000)
    {
      Serial.println("Timeout: No key press for 60 seconds, resetting.");
      inputNumber = "";
      currentState = SHOW_CURRENT_TIME;
    }
  }
}

// Execute current state actions
void executeState()
{
  switch (currentState)
  {
  case SHOW_CURRENT_TIME:
    rtc.refresh();
    {
      int adjustedHour = rtc.hour();
      int adjustedDay = rtc.day();
      applyEasternTimeOffset(adjustedHour, adjustedDay, rtc.month(), rtc.year());
      displayCurrentTime(adjustedHour, rtc.minute(), rtc.second());
    }
    break;

  case WAITING_INPUT:
    // Just show colon, waiting for input
    display.setSegments(onlyCollon);
    break;

  case SETTING_TIMER:
    // Display is already updated in handleInput()
    // Nothing else to do here
    break;

  case COUNTDOWN:
    // Tick countdown every second (non-blocking)
    if (nonBlockingDelay(countdownTickMillis, 1000))
    {
      if (timerSeconds == 0)
      {
        if (timerMinutes > 0)
        {
          timerMinutes--;
          timerSeconds = 59;
        }
      }
      else
      {
        timerSeconds--;
      }

      // Log to serial
      if (timerMinutes < 10)
        Serial.print("0");
      Serial.print(timerMinutes);
      Serial.print(":");
      if (timerSeconds < 10)
        Serial.print("0");
      Serial.println(timerSeconds);

      updateDisplay(timerMinutes, timerSeconds);

      // Check if countdown complete
      if (timerMinutes == 0 && timerSeconds == 0)
      {
        currentState = COMPLETE;
      }
    }
    break;

  case COMPLETE:
  {
    // Blink 00:00 four times
    int blinkCount = 0;
    while (blinkCount < 4)
    {
      display.showNumberDecEx(0, 0b01000000, true, 4, 0); // Show 00:00
      delay(500);
      display.setSegments(onlyCollon); // Clear display
      delay(200);
      blinkCount++;
    }
    currentState = SHOW_CURRENT_TIME;
    break;
  }
  }
}

void setup()
{
  Serial.begin(9600);

  // Set the brightness to 5 (0=dimmest 7=brightest)
  display.setBrightness(0);

  // prints 00:00 at start
  // display.showNumberDecEx(0, 0b01000000, true, 4, 0);

  // Set all segments ON
  display.setSegments(allON);
  delay(100);
  display.setSegments(onlyCollon);

  // Initialize I2C bus for RTC communication
  URTCLIB_WIRE.begin();

  // Start in show time state
  currentState = SHOW_CURRENT_TIME;

  // Comment out below line once you set the date & time (set the time as UTC)
  // Following line sets the RTC with an explicit date & time
  // for example to set April 14 2025 at 12:56 you would call:
  // rtc.set(second, minute, hour, dayOfWeek, dayOfMonth, month, year)
  // rtc.set(0, 43, 3, 1, 16, 11, 25);
  // set day of week (1=Sunday, 7=Saturday)
  Serial.println("Current RTC Date and Time (UTC):");
  rtc.refresh();
  Serial.print(rtc.day());
  Serial.print("/");
  Serial.print(rtc.month());
  Serial.print("/");
  Serial.print(rtc.year());
  Serial.print(" ");
  Serial.print(rtc.hour());
  Serial.print(":");
  Serial.print(rtc.minute());
  Serial.print(":");
  Serial.println(rtc.second());
}

void loop()
{
  // --- State Machine Implementation ---
  handleInput();
  executeState();
}

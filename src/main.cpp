#include <Arduino.h>

#include <Keypad.h>
#include <TM1637Display.h>

// https://lastminuteengineers.com/ds3231-rtc-arduino-tutorial/
#include "uRTCLib.h"

#include <Adafruit_NeoPixel.h>

// Definition of all digital pins

// ***** Keypad input pins *****
// Mapping https://learn.adafruit.com/matrix-keypad/pinouts
// COL2, ROW1, COL1, ROW4, COL3, ROW3, ROW2
const byte ROWS = 4; // four rows
const byte COLS = 3; // three columns
char keys[ROWS][COLS] = {
    {'1', '2', '3'},
    {'4', '5', '6'},
    {'7', '8', '9'},
    {'*', '0', '#'}};
byte rowPins[ROWS] = {3, 8, 7, 5}; // connect to the row pinouts of the keypad
byte colPins[COLS] = {4, 2, 6};    // connect to the column pinouts of the keypad

// ***** Output pins *****
// TM1637 clock pin
const  uint8_t  TM1637CLKPin = 9;

// TM1637 data pin
const  uint8_t  TM1637DataPin = 10;

// Buzzer
const  uint8_t  BuzzerPin = 11;

// Which pin on the Arduino is connected to the NeoPixels?
const uint8_t  NeoPixelPin = 13;

// How many NeoPixels are attached to the Arduino?
const uint8_t  NeoPixelCount = 24;

// ***** Keypad segment definitions *****

// Create an array that turns all segments ON
const uint8_t allON[] = {0xff, 0xff, 0xff, 0xff};

// Create an array that displays only the collon
const uint8_t onlyCollon[] = {0x00, SEG_DP, 0x00, 0x00};

// ***** Objects *****

// Create a display object of type TM1637Display
TM1637Display display = TM1637Display(TM1637CLKPin, TM1637DataPin);

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// DS3231 RTC object
uRTCLib rtc(0x68);

// Rotator class based on https://www.arduinoslovakia.eu/blog/2021/10/neopixel-ring-rotator?lang=en
// to create a rotating color pattern on the NeoPixel ring
// for high-level orchestration of Rotating pattern
// For more patterns or low-level code, reference https://github.com/RoboUlbricht/arduinoslovakia/blob/master/neopixel/hsv_red_circle_rotate/hsv_red_circle_rotate.ino
class Rotator
{
    Adafruit_NeoPixel *strip;
    int position;
    uint16_t hue;
    uint8_t saturation;

  public:
    Rotator(Adafruit_NeoPixel *npx, int pos = 0, uint16_t h = 0, uint8_t sat = 255);
    void Step();
};

Rotator::Rotator(Adafruit_NeoPixel *npx, int pos, uint16_t h, uint8_t sat)
  : strip(npx), position(pos), hue(h), saturation(sat)
{
}

void Rotator::Step()
{
  // hue - 0-65535
  // saturation - 0-255
  // value - 0-255
  for (int i = 0; i < NeoPixelCount; i++)
    strip->setPixelColor(
      (i + position) % NeoPixelCount,
      // strip->ColorHSV(hue, saturation, strip->gamma8(pow(i, 2) * (255 / pow(NeoPixelCount, 2))))
      strip->ColorHSV(hue, saturation, strip->gamma8(i * (255 / NeoPixelCount)))
    );
  strip->show();
  position++;
  position %= NeoPixelCount;
}

Adafruit_NeoPixel ring(NeoPixelCount, NeoPixelPin, NEO_GRB + NEO_KHZ800);

// Rotator rt(&ring, 0, 0, 255); // red
// Rotator rt(&ring, 0, 0, 200); // pink
// Rotator rt(&ring, 0, 40000L, 200); // lightblue
Rotator rt(&ring, 0, 4000L, 255); // yellow

// ***** Variables *****

// State machine definitions
enum State {
  SHOW_CURRENT_TIME,    // Display current time from RTC
  SHOW_CURRENT_TIME_NO_LED, // Display current time from RTC, no LEDs
  WAITING_INPUT,        // Waiting for input (colon only)
  SETTING_TIMER,        // User is entering timer digits
  COUNTDOWN,            // Timer is actively counting down
  COMPLETE              // Timer finished, beeping/blinking
};

State currentState = SHOW_CURRENT_TIME;
int timerMinutes = 0;           // countdown minutes
int timerSeconds = 0;           // countdown seconds
unsigned long countdownTickMillis = 0; // Timer for countdown ticks
unsigned long ledTickMillis = 0; // Timer for LED animation
unsigned long showCurrentTimeEntryMillis = 0; // Timer for SHOW_CURRENT_TIME state duration
int ledIndex = 0; // Current LED position
String inputNumber = ""; // Variable to store the number sequence

// COMPLETE state blink animation tracking
unsigned long blinkTickMillis = 0; // Timer for blink animation
int blinkCount = 0; // Counter for blink cycles
bool buzzerOn = false; // Track buzzer state


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
  // Convert to 12-hour format
  int displayHours = (hours % 12 == 0) ? 12 : hours % 12;

  // concatenate hours and minutes for display
  int displayTime = displayHours * 100 + minutes;

  // Define the bitmask for the colon. 0b01000000 is for the colon between digits 1 and 2.
  // To make it flash, you can toggle this value every second.
  uint8_t colonMask = (seconds % 2 == 0) ? 0x40 : 0;

  display.showNumberDecEx(displayTime, colonMask, false, 4, 0);
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
    case SHOW_CURRENT_TIME_NO_LED:
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

          // Reset LED animation
          ledTickMillis = millis();
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
        showCurrentTimeEntryMillis = millis();
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
        showCurrentTimeEntryMillis = millis();
      }
      break;

    case COMPLETE:
      // Any key press exits complete state
      digitalWrite(BuzzerPin, LOW);  // Ensure buzzer is off
      buzzerOn = false;              // Reset buzzer state
      currentState = SHOW_CURRENT_TIME;
      showCurrentTimeEntryMillis = millis();
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
      showCurrentTimeEntryMillis = millis();
    }
  }
}

// Execute current state actions
void executeState()
{
  switch (currentState)
  {
  case SHOW_CURRENT_TIME:
    if (nonBlockingDelay(showCurrentTimeEntryMillis, 15000)) // After 15 seconds, switch to no-LED mode
    {
      currentState = SHOW_CURRENT_TIME_NO_LED;
    }
    
    ring.fill(ring.ColorHSV(4000L, 255, 50)); // Fill with Yellow color for standby indication
    ring.show();

    rtc.refresh();
    {
      int adjustedHour = rtc.hour();
      int adjustedDay = rtc.day();
      applyEasternTimeOffset(adjustedHour, adjustedDay, rtc.month(), rtc.year());
      displayCurrentTime(adjustedHour, rtc.minute(), rtc.second());
    }
    break;

  case SHOW_CURRENT_TIME_NO_LED:
    ring.clear();
    ring.show();

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
    // LED Animation
    if (nonBlockingDelay(ledTickMillis, 75))
    {
      rt.Step();
    }

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
        // Initialize blink animation
        blinkTickMillis = millis();
        blinkCount = 0;
        buzzerOn = false;  // Start with OFF so first iteration turns it ON
      }
    }
    break;

  case COMPLETE:
  {
    ring.fill(ring.ColorHSV(4000L, 255, 50)); // Fill with Yellow color for complete indication
    ring.show();

    // Blink 00:00 four times using non-blocking delays
    if (nonBlockingDelay(blinkTickMillis, buzzerOn ? 500 : 200))
    {
      if (buzzerOn)
      {
        // Turn off buzzer and show colon
        digitalWrite(BuzzerPin, LOW);
        display.setSegments(onlyCollon);
        buzzerOn = false;
        blinkCount++;  // Increment after each complete ON phase
      }
      else
      {
        // Turn on buzzer and show 00:00
        digitalWrite(BuzzerPin, HIGH);
        display.showNumberDecEx(0, 0b01000000, true, 4, 0);
        buzzerOn = true;
      }

      // Check if blink sequence is complete
      if (blinkCount >= 4)
      {
        digitalWrite(BuzzerPin, LOW);
        blinkCount = 0;
        currentState = SHOW_CURRENT_TIME;
        showCurrentTimeEntryMillis = millis();
      }
    }
    break;
  }
  }
}

void setup()
{
  Serial.begin(9600);

  // Setup display

  // Set the brightness to 5 (0=dimmest 7=brightest)
  display.setBrightness(0);

  // prints 00:00 at start
  // display.showNumberDecEx(0, 0b01000000, true, 4, 0);

  // Set all segments ON
  display.setSegments(allON);
  delay(100);
  display.setSegments(onlyCollon);

  // Setup RTC

  // Initialize I2C bus for RTC communication
  URTCLIB_WIRE.begin();

  // Start in show time state
  currentState = SHOW_CURRENT_TIME;
  showCurrentTimeEntryMillis = millis();

  // Comment out below line once you set the date & time (set the time as UTC)
  // Following line sets the RTC with an explicit date & time
  // for example to set April 14 2025 at 12:56 you would call:
  // rtc.set(second, minute, hour, dayOfWeek, dayOfMonth, month, year)
  // rtc.set(0, 18, 18, 2, 5, 1, 26);
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

  // Setup buzzer
  pinMode(BuzzerPin,  OUTPUT);

  Serial.println("Setting up NeoPixels...");
  // Setup NeoPixels
  ring.begin(); // Initialize NeoPixel library
  ring.show();  // Initialize all pixels to 'off'
  Serial.println("NeoPixels setup complete.");
}

#define MAXHUE 256*6

int position = 0;


void loop()
{
  // --- State Machine Implementation ---
  handleInput();
  executeState();
}

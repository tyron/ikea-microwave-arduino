#include <Keypad.h>
#include <TM1637Display.h>

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
}

void loop(){
  char key = keypad.getKey();

  if (key) {
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
      }
      inputNumber = ""; // Reset the input number after countdown
    } else if (key == '*') { // If the key is '*', clear the input
      inputNumber = "";
      Serial.println("Input cleared.");
      display.setSegments(onlyCollon);
    }
  }
}

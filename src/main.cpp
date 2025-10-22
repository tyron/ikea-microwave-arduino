/* @file HelloKeypad.pde
|| @version 1.0
|| @author Alexander Brevig
|| @contact alexanderbrevig@gmail.com
||
|| @description
|| | Demonstrates the simplest use of the matrix Keypad library.
|| #
*/
#include <Keypad.h>

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

void setup(){
  Serial.begin(9600);
}
  
bool nonBlockingDelay(unsigned long &previousMillis, unsigned long interval) {
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    return true;
  }
  return false;
}

void loop(){
  char key = keypad.getKey();

  if (key) {
    if (key >= '0' && key <= '9') { // If the key is a number
      inputNumber += key; // Append the number to the input string
      Serial.println(key);
    } else if (key == '#') { // If the key is '#'
      if (inputNumber.length() > 0) { // Ensure there is a number to count down from
        int totalSeconds = inputNumber.toInt(); // Convert the string to total seconds
        int minutes = totalSeconds / 100; // Extract minutes
        int seconds = totalSeconds % 100; // Extract seconds

        // Adjust seconds if they exceed 59
        minutes += seconds / 60;
        seconds = seconds % 60;

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
          }

          char interruptKey = keypad.getKey(); // Check for interrupt key
          if (interruptKey == '*') { // If '*' is pressed, stop the countdown
            Serial.println("Countdown stopped.");
            break;
          }
        }
      }
      inputNumber = ""; // Reset the input number after countdown
    }
  }
}

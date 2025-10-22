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
  
void loop(){
  char key = keypad.getKey();

  if (key) {
    if (key >= '0' && key <= '9') { // If the key is a number
      inputNumber += key; // Append the number to the input string
      Serial.println(key);
    } else if (key == '#') { // If the key is '#'
      if (inputNumber.length() > 0) { // Ensure there is a number to count down from
        int countdown = inputNumber.toInt(); // Convert the string to an integer
        unsigned long previousMillis = millis(); // Store the current time
        while (countdown >= 0) {
          unsigned long currentMillis = millis();
          if (currentMillis - previousMillis >= 1000) { // Check if 1 second has passed
            previousMillis = currentMillis; // Update the previous time
            Serial.println(countdown);
            countdown--;
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

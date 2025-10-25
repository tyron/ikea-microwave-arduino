#ifndef TIMER_LOGIC_H
#define TIMER_LOGIC_H

#ifndef UNIT_TEST
#include <Arduino.h>
#else
// For unit testing on native platform
#include <stdint.h>
extern unsigned long millis(); // Will be mocked in test file
#endif

// Structure to hold parsed timer values
struct TimerValue {
  int minutes;
  int seconds;
  bool valid;
};

// Non-blocking delay function
// Returns true when the specified interval has elapsed
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

// Parse timer input string to minutes and seconds
// Input format: "MMSS" as a string (e.g., "130" = 1:30, "2545" = 25:45)
// Automatically normalizes seconds > 59
TimerValue parseTimerInput(const char* inputStr)
{
  TimerValue result = {0, 0, false};

  if (inputStr == nullptr || inputStr[0] == '\0')
  {
    return result; // Invalid input
  }

  // Convert string to integer
  int totalValue = 0;
  for (int i = 0; inputStr[i] != '\0'; i++)
  {
    if (inputStr[i] < '0' || inputStr[i] > '9')
    {
      return result; // Invalid character
    }
    totalValue = totalValue * 10 + (inputStr[i] - '0');
  }

  // Parse as MMSS format
  result.minutes = totalValue / 100;
  result.seconds = totalValue % 100;

  // Normalize seconds > 59 (e.g., 1:75 becomes 2:15)
  result.minutes += result.seconds / 60;
  result.seconds = result.seconds % 60;

  result.valid = true;
  return result;
}

// Decrement timer by one second
// Returns true if timer has reached zero
bool decrementTimer(int &minutes, int &seconds)
{
  if (seconds == 0)
  {
    if (minutes > 0)
    {
      minutes--;
      seconds = 59;
      return false;
    }
    else
    {
      // Timer has reached 0:00
      return true;
    }
  }
  else
  {
    seconds--;
    return false;
  }
}

#endif // TIMER_LOGIC_H

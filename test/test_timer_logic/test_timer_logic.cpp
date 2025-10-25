#include <unity.h>
#include "timer_logic.h"

// Override millis() for testing purposes
static unsigned long mock_millis_value = 0;
unsigned long millis() {
    return mock_millis_value;
}

void setUp(void) {
    // Reset before each test
    mock_millis_value = 0;
}

void tearDown(void) {
    // Clean up after each test
}

// ===== Tests for parseTimerInput =====

void test_parseTimerInput_simple_case(void) {
    TimerValue result = parseTimerInput("130");
    TEST_ASSERT_TRUE(result.valid);
    TEST_ASSERT_EQUAL_INT(1, result.minutes);
    TEST_ASSERT_EQUAL_INT(30, result.seconds);
}

void test_parseTimerInput_four_digits(void) {
    TimerValue result = parseTimerInput("2545");
    TEST_ASSERT_TRUE(result.valid);
    TEST_ASSERT_EQUAL_INT(25, result.minutes);
    TEST_ASSERT_EQUAL_INT(45, result.seconds);
}

void test_parseTimerInput_seconds_normalization(void) {
    // Input: 1:75 should normalize to 2:15
    TimerValue result = parseTimerInput("175");
    TEST_ASSERT_TRUE(result.valid);
    TEST_ASSERT_EQUAL_INT(2, result.minutes);
    TEST_ASSERT_EQUAL_INT(15, result.seconds);
}

void test_parseTimerInput_extreme_seconds_normalization(void) {
    // Input: 1:99 should normalize to 2:39
    TimerValue result = parseTimerInput("199");
    TEST_ASSERT_TRUE(result.valid);
    TEST_ASSERT_EQUAL_INT(2, result.minutes);
    TEST_ASSERT_EQUAL_INT(39, result.seconds);
}

void test_parseTimerInput_single_digit(void) {
    TimerValue result = parseTimerInput("5");
    TEST_ASSERT_TRUE(result.valid);
    TEST_ASSERT_EQUAL_INT(0, result.minutes);
    TEST_ASSERT_EQUAL_INT(5, result.seconds);
}

void test_parseTimerInput_two_digits(void) {
    TimerValue result = parseTimerInput("45");
    TEST_ASSERT_TRUE(result.valid);
    TEST_ASSERT_EQUAL_INT(0, result.minutes);
    TEST_ASSERT_EQUAL_INT(45, result.seconds);
}

void test_parseTimerInput_empty_string(void) {
    TimerValue result = parseTimerInput("");
    TEST_ASSERT_FALSE(result.valid);
}

void test_parseTimerInput_null_pointer(void) {
    TimerValue result = parseTimerInput(nullptr);
    TEST_ASSERT_FALSE(result.valid);
}

void test_parseTimerInput_zero(void) {
    TimerValue result = parseTimerInput("0");
    TEST_ASSERT_TRUE(result.valid);
    TEST_ASSERT_EQUAL_INT(0, result.minutes);
    TEST_ASSERT_EQUAL_INT(0, result.seconds);
}

void test_parseTimerInput_large_value(void) {
    // Input: 9999 = 99:99 = 100:39 after normalization
    TimerValue result = parseTimerInput("9999");
    TEST_ASSERT_TRUE(result.valid);
    TEST_ASSERT_EQUAL_INT(100, result.minutes);
    TEST_ASSERT_EQUAL_INT(39, result.seconds);
}

// ===== Tests for decrementTimer =====

void test_decrementTimer_normal_countdown(void) {
    int minutes = 5;
    int seconds = 30;

    bool finished = decrementTimer(minutes, seconds);

    TEST_ASSERT_FALSE(finished);
    TEST_ASSERT_EQUAL_INT(5, minutes);
    TEST_ASSERT_EQUAL_INT(29, seconds);
}

void test_decrementTimer_second_rollover(void) {
    int minutes = 2;
    int seconds = 0;

    bool finished = decrementTimer(minutes, seconds);

    TEST_ASSERT_FALSE(finished);
    TEST_ASSERT_EQUAL_INT(1, minutes);
    TEST_ASSERT_EQUAL_INT(59, seconds);
}

void test_decrementTimer_reaches_zero(void) {
    int minutes = 0;
    int seconds = 1;

    bool finished = decrementTimer(minutes, seconds);

    TEST_ASSERT_FALSE(finished);
    TEST_ASSERT_EQUAL_INT(0, minutes);
    TEST_ASSERT_EQUAL_INT(0, seconds);
}

void test_decrementTimer_already_zero(void) {
    int minutes = 0;
    int seconds = 0;

    bool finished = decrementTimer(minutes, seconds);

    TEST_ASSERT_TRUE(finished);
    TEST_ASSERT_EQUAL_INT(0, minutes);
    TEST_ASSERT_EQUAL_INT(0, seconds);
}

void test_decrementTimer_multiple_decrements(void) {
    int minutes = 1;
    int seconds = 2;

    // First decrement: 1:02 -> 1:01
    bool finished = decrementTimer(minutes, seconds);
    TEST_ASSERT_FALSE(finished);
    TEST_ASSERT_EQUAL_INT(1, minutes);
    TEST_ASSERT_EQUAL_INT(1, seconds);

    // Second decrement: 1:01 -> 1:00
    finished = decrementTimer(minutes, seconds);
    TEST_ASSERT_FALSE(finished);
    TEST_ASSERT_EQUAL_INT(1, minutes);
    TEST_ASSERT_EQUAL_INT(0, seconds);

    // Third decrement: 1:00 -> 0:59
    finished = decrementTimer(minutes, seconds);
    TEST_ASSERT_FALSE(finished);
    TEST_ASSERT_EQUAL_INT(0, minutes);
    TEST_ASSERT_EQUAL_INT(59, seconds);
}

// ===== Tests for nonBlockingDelay =====

void test_nonBlockingDelay_not_elapsed(void) {
    unsigned long previousMillis = 0;
    mock_millis_value = 500;

    bool result = nonBlockingDelay(previousMillis, 1000);

    TEST_ASSERT_FALSE(result);
    TEST_ASSERT_EQUAL_UINT32(0, previousMillis); // Should not update
}

void test_nonBlockingDelay_exactly_elapsed(void) {
    unsigned long previousMillis = 0;
    mock_millis_value = 1000;

    bool result = nonBlockingDelay(previousMillis, 1000);

    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL_UINT32(1000, previousMillis); // Should update to current time
}

void test_nonBlockingDelay_more_than_elapsed(void) {
    unsigned long previousMillis = 0;
    mock_millis_value = 1500;

    bool result = nonBlockingDelay(previousMillis, 1000);

    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL_UINT32(1500, previousMillis);
}

void test_nonBlockingDelay_consecutive_calls(void) {
    unsigned long previousMillis = 0;

    // First call - not elapsed
    mock_millis_value = 500;
    bool result = nonBlockingDelay(previousMillis, 1000);
    TEST_ASSERT_FALSE(result);

    // Second call - elapsed
    mock_millis_value = 1000;
    result = nonBlockingDelay(previousMillis, 1000);
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL_UINT32(1000, previousMillis);

    // Third call - not elapsed yet
    mock_millis_value = 1500;
    result = nonBlockingDelay(previousMillis, 1000);
    TEST_ASSERT_FALSE(result);

    // Fourth call - elapsed again
    mock_millis_value = 2000;
    result = nonBlockingDelay(previousMillis, 1000);
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL_UINT32(2000, previousMillis);
}

int main(int argc, char **argv) {
    UNITY_BEGIN();

    // parseTimerInput tests
    RUN_TEST(test_parseTimerInput_simple_case);
    RUN_TEST(test_parseTimerInput_four_digits);
    RUN_TEST(test_parseTimerInput_seconds_normalization);
    RUN_TEST(test_parseTimerInput_extreme_seconds_normalization);
    RUN_TEST(test_parseTimerInput_single_digit);
    RUN_TEST(test_parseTimerInput_two_digits);
    RUN_TEST(test_parseTimerInput_empty_string);
    RUN_TEST(test_parseTimerInput_null_pointer);
    RUN_TEST(test_parseTimerInput_zero);
    RUN_TEST(test_parseTimerInput_large_value);

    // decrementTimer tests
    RUN_TEST(test_decrementTimer_normal_countdown);
    RUN_TEST(test_decrementTimer_second_rollover);
    RUN_TEST(test_decrementTimer_reaches_zero);
    RUN_TEST(test_decrementTimer_already_zero);
    RUN_TEST(test_decrementTimer_multiple_decrements);

    // nonBlockingDelay tests
    RUN_TEST(test_nonBlockingDelay_not_elapsed);
    RUN_TEST(test_nonBlockingDelay_exactly_elapsed);
    RUN_TEST(test_nonBlockingDelay_more_than_elapsed);
    RUN_TEST(test_nonBlockingDelay_consecutive_calls);

    return UNITY_END();
}

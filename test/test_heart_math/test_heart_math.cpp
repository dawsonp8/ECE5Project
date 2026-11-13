// Host unit tests for the heart-rate math (stage 4). Run: pio test -e native
#include <unity.h>
#include "heart_math.h"

void setUp() {}
void tearDown() {}

void test_ibi_range() {
  TEST_ASSERT_FALSE(ibiValid(299));
  TEST_ASSERT_TRUE(ibiValid(300));   // 200 BPM
  TEST_ASSERT_TRUE(ibiValid(1500));  // 40 BPM
  TEST_ASSERT_FALSE(ibiValid(1501));
  TEST_ASSERT_FALSE(ibiValid(0));
  TEST_ASSERT_FALSE(ibiValid(-5));
}

void test_bpm_from_ibi() {
  TEST_ASSERT_EQUAL_INT16(60, bpmFromIbi(1000));
  TEST_ASSERT_EQUAL_INT16(75, bpmFromIbi(800));
  TEST_ASSERT_EQUAL_INT16(200, bpmFromIbi(300));
  TEST_ASSERT_EQUAL_INT16(40, bpmFromIbi(1500));
  TEST_ASSERT_EQUAL_INT16(71, bpmFromIbi(845));  // 71.0 -> 71
  TEST_ASSERT_EQUAL_INT16(0, bpmFromIbi(0));
}

void test_move_time_at_or_below_rest_is_max() {
  TEST_ASSERT_EQUAL_INT32(10000, moveTimeMs(70, 70));
  TEST_ASSERT_EQUAL_INT32(10000, moveTimeMs(55, 70));
}

void test_move_time_slope() {
  TEST_ASSERT_EQUAL_INT32(9800, moveTimeMs(71, 70));
  TEST_ASSERT_EQUAL_INT32(8000, moveTimeMs(80, 70));
  TEST_ASSERT_EQUAL_INT32(6000, moveTimeMs(90, 70));
}

void test_move_time_floor() {
  TEST_ASSERT_EQUAL_INT32(3000, moveTimeMs(105, 70));  // +35 BPM hits the floor
  TEST_ASSERT_EQUAL_INT32(3200, moveTimeMs(104, 70));
  TEST_ASSERT_EQUAL_INT32(3000, moveTimeMs(200, 60));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_ibi_range);
  RUN_TEST(test_bpm_from_ibi);
  RUN_TEST(test_move_time_at_or_below_rest_is_max);
  RUN_TEST(test_move_time_slope);
  RUN_TEST(test_move_time_floor);
  return UNITY_END();
}

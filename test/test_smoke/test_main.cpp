#include <unity.h>
#include "core.h"

void setUp() {}
void tearDown() {}

void test_core_wiring() { TEST_ASSERT_EQUAL(1, core_version()); }

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_core_wiring);
  return UNITY_END();
}

#include <limits.h>
#include <stdint.h>

__attribute__((noinline)) static uint32_t exercise_sound_rules(int32_t x,
                                                                int32_t y) {
  int32_t positive = x > 1 ? x : 1;
  int32_t nonnegative = y > 0 ? y : 0;
  int32_t negative = x < -1 ? x : -1;
  int32_t nonpositive = y < 0 ? y : 0;

  int32_t positive_identity = positive * 1;
  int32_t nonnegative_quotient = nonnegative / positive;
  int32_t negative_identity = negative * 1;
  int32_t nonpositive_quotient = nonpositive / positive;
  int32_t positive_plus_zero = positive + 0;
  int32_t one_plus_negative = 1 + negative;

  return (uint32_t)positive_identity ^ (uint32_t)nonnegative_quotient ^
         (uint32_t)negative_identity ^ (uint32_t)nonpositive_quotient ^
         (uint32_t)positive_plus_zero ^ (uint32_t)one_plus_negative;
}

__attribute__((noinline)) int32_t add_positive(int32_t lhs, int32_t rhs) {
  int32_t positive_lhs = lhs > 1 ? lhs : 1;
  int32_t positive_rhs = rhs > 1 ? rhs : 1;
  return positive_lhs + positive_rhs;
}

// These operations intentionally cross signed boundaries. ExtendedSign must
// leave their results unknown because their mathematical sign does not survive
// fixed-width wrapping.
__attribute__((noinline)) int32_t unchecked_add(int32_t value) {
  int32_t positive = value > 1 ? value : 1;
  return positive + 1;
}

__attribute__((noinline)) int32_t unchecked_subtract(int32_t value) {
  int32_t negative = value < -1 ? value : -1;
  return negative - 1;
}

__attribute__((noinline)) int32_t unchecked_multiply(int32_t value) {
  int32_t positive = value > 1 ? value : 1;
  return positive * 2;
}

__attribute__((noinline)) int32_t unchecked_divide(int32_t value) {
  int32_t negative = value < -1 ? value : -1;
  return negative / -1;
}

int main(void) {
  static const int32_t edges[] = {INT_MIN, INT_MIN + 1, -2, -1,
                                  0,       1,           2,  INT_MAX};
  volatile uint32_t checksum = 0;
  for (unsigned i = 0; i < sizeof(edges) / sizeof(edges[0]); ++i)
    for (unsigned j = 0; j < sizeof(edges) / sizeof(edges[0]); ++j)
      checksum ^= exercise_sound_rules(edges[i], edges[j]);

  uint32_t state = 0x6d2b79f5u;
  for (unsigned i = 0; i < 1000000; ++i) {
    state = state * 1664525u + 1013904223u;
    int32_t x = (int32_t)state;
    state = state * 1664525u + 1013904223u;
    int32_t y = (int32_t)state;
    checksum ^= exercise_sound_rules(x, y);
  }

  volatile int32_t max_value = INT_MAX;
  volatile int32_t min_value = INT_MIN;
  checksum ^= (uint32_t)unchecked_add(max_value);
  checksum ^= (uint32_t)unchecked_subtract(min_value);
  checksum ^= (uint32_t)unchecked_multiply(max_value);

  // INT_MIN / -1 is undefined in both C and LLVM IR. Exercise the same
  // transfer-function shape with the closest safe dividend instead.
  checksum ^= (uint32_t)unchecked_divide(min_value + 1);

  volatile int32_t result = add_positive(max_value, max_value);
  return result == -2 || checksum == UINT32_MAX ? 0 : 1;
}

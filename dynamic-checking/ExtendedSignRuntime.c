#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static const char *fact_name(int32_t kind) {
  switch (kind) {
  case 1:
    return "one";
  case 2:
    return "negative";
  case 3:
    return "zero";
  case 4:
    return "positive";
  case 5:
    return "nonnegative";
  case 6:
    return "nonpositive";
  default:
    return "unknown";
  }
}

void __extended_sign_assert(_Bool condition, int32_t kind, uint64_t id,
                            const char *file, int32_t line, int32_t column) {
  if (condition)
    return;

  fprintf(stderr,
          "extended-sign assertion failed: check #%" PRIu64
          " expected %s at %s:%" PRId32 ":%" PRId32 "\n",
          id, fact_name(kind), file, line, column);
  abort();
}

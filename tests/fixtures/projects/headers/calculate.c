#include "calculate.h"

static int offset() { return 1; }

int calculate(int value) {
    return adjust(value * 2) + offset();
}

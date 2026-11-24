#include "calculate.h"
#include "adjust.h"

static int offset() { return 2; }

int main() {
    if (calculate(20) != 42) return 1;
    if (adjust(40) != 41) return 2;
    return offset() - 2;
}

#include "calculate.h"
#include "custom.h"

int main() {
    if (calculate(0.0, 0, 0.0, 0, 0.0, 0, 0.0, 0, 0.0, 0,
                  0.0, 0, 0.0, 0, 0.0, 0, 40.0, 2) != 42.0) return 1;
    if (printf(42) != 42) return 2;
    return 0;
}

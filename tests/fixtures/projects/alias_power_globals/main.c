#include "shared.h"

int main() {
    if (initial != 16.0 || results[0] != 0.0 || latest != 0 || changes != 0) return 1;
    if (populate() != 0) return 2;
    if (results[0] != 4.0 || results[1] != -8.0 || results[2] != 0.25) return 3;
    if (latest != results + 1 || *latest != -8.0 || changes != 1) return 4;
    if (resize_results() != 0) return 5;
    if (results[0] != 4.0 || results[1] != -8.0 || results[2] != 0.25) return 6;
    if (latest != results + 3 || *latest != 16.0 || changes != 2) return 7;
    return 0;
}

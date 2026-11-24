#include "shared.h"

Count populate() {
    results[0] = initial**0.5;
    results[1] = (-2)**3;
    results[2] = 2**-2;
    latest = results + 1;
    changes = changes + 1;
    return 0;
}

Count resize_results() {
    latest = results + 3;
    *latest = results[0]**2;
    changes = changes + 1;
    return 0;
}

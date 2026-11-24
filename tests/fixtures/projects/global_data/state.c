#include "project.h"

struct Counter { int value; };
static struct Counter counter;
static int initial = 42;

int next_value() {
    int result = initial + counter.value;
    counter.value = counter.value + 1;
    return result;
}

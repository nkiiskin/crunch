#include "data.h"

int resize_arrays() {
    message[0] = 'H';
    if (message[0] != 'H' || message[1] != 'i') return 1;
    numbers[0] = 42;
    numbers[1] = 7;
    big[0] = 123;
    fractions[0] = 2.5;
    entries[0].marker = (char)9;
    entries[0].value = 37;
    entries[0].tag = 99;
    return 0;
}

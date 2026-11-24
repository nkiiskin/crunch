#include "data.h"
#include <string.h>

int main() {
    char *original = message;
    if (message[0] != 'h' || empty[0] != 0) return 1;
    if (resize_arrays() != 0) return 2;
    message[0] = 'X';
    if (original[0] != 'X') return 3;
    message[0] = 'H';
    if (original[0] != 'H' || original[1] != 'i') return 7;
    if (numbers[1] != 7 || big[0] != 123 ||
        fractions[0] != 2.5) return 4;
    if (entries[0].marker != (char)9 || entries[0].tag != 99) return 5;
    memcpy(empty, message, 3);
    entries[1].value = 42;
    if (check_arrays() != 0) return 6;
    return 0;
}

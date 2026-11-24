#include <stdio.h>
#include "io.h"

int interact() {
    char text[32];
    if (sizeof(text) != 32) return -1;
    int count = input(text, 8);
    printf("You said: %s\n", text);
    return count;
}

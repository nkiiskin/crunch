#include "data.h"

char message[8] = "hi";
char empty[8];
int numbers[2];
long big[1];
double fractions[1];
struct entry entries[2];

int check_arrays() {
    if (message[0] != 'H' || message[1] != 'i') return 1;
    if (empty[0] != 'H' || empty[2] != 0) return 2;
    if (numbers[0] != 42) return 3;
    if (entries[0].value != 37 || entries[1].value != 42) return 4;
    return 0;
}

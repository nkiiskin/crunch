#include "data.h"
#include <string.h>

static char local[10] = "123456";

int resize_text(char *text) {
    strcpy(local, text);
    if (local[0] != 'h' || local[4] != 'o') return 1;
    local[0] = 'H';
    return 42;
}

#include <string.h>

extern int __crunch_fallback_sprintf(char *, const char *, ...);

int main(void) {
    char buffer[128];
    int result = __crunch_fallback_sprintf(buffer, "%d %d %d %d %d %f %s",
                                            1, 2, 3, 4, 5, 1.5, "ok");
    return result != 21 || strcmp(buffer, "1 2 3 4 5 1.500000 ok");
}

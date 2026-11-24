#include "project.h"

int main() {
    show_message();
    if (next_value() != 42) return 1;
    if (next_value() != 43) return 2;
    return 0;
}

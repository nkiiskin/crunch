#include "data.h"

int main() {
    if (resize_text(get_text()) != 42) return 1;
    if (get_text()[0] != 'h') return 2;
    return 0;
}

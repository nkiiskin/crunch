#include <stdio.h>
#include "project.h"

static char message[] = "Hello from a module!\n";

void show_message() {
    printf("%s", message);
}

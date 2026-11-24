#include "reader.h"
#include <unistd.h>

int main(int argc, char** argv) {
    if (argc != 2) return 1;
    return copy_file(argv[1], STDOUT_FILENO);
}

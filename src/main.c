#include <stdio.h>

#include "driver/driver.h"

/**
 * @brief Dispatches top-level command-line requests to compiler tools.
 * @param argc Number of command-line arguments.
 * @param argv Command-line argument vector.
 * @return Process exit status: zero for success and nonzero for failure.
 */
int main(int argc, char **argv)
{
    CompilerResult result = compiler_run(argc, argv);

    if (result.status != COMPILER_OK) {
        return 1;
    }

    return 0;
}

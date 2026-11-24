#ifndef CRUNCH_DRIVER_DRIVER_H
#define CRUNCH_DRIVER_DRIVER_H

#include "common/compiler.h"

/**
 * @brief Runs the compiler command-line pipeline.
 * @param argc Number of command-line arguments.
 * @param argv Command-line argument vector.
 * @return Compilation status and accumulated error and warning counts.
 */
CompilerResult compiler_run(int argc, char **argv);

#endif

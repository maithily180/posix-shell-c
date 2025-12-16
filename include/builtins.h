#ifndef BUILTINS_H
#define BUILTINS_H

#include <stdbool.h>

// Try to handle a builtin. Returns true if handled (and nothing else should run).
bool try_handle_builtin(char **argv, int argc);

#endif



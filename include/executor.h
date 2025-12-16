#ifndef EXECUTOR_H
#define EXECUTOR_H

#include <stdbool.h>

// Execute only the first atomic of the first cmd_group from a valid input string.
// Returns true if something was executed/handled, false otherwise.
bool execute_first_atomic(const char *input);

// Execute the first cmd_group including pipelines and redirections.
bool execute_first_group_pipeline(const char *input);

// Execute entire shell_cmd with sequential execution (;)
bool execute_shell_cmd(const char *input);

#endif


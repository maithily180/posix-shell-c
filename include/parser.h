#ifndef PARSER_H
#define PARSER_H

#include <stdbool.h>

// Returns true if the input matches the grammar, false otherwise.
bool parser_is_valid_command(const char *input);

#endif



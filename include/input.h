#ifndef INPUT_H
#define INPUT_H

#include <stddef.h>

// Reads a line from stdin. Returns a malloc'd string without trailing newline.
// Returns NULL on EOF or error.
char *read_line(void);

#endif



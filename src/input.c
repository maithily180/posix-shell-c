#include "input.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

char *read_line(void) {
	char *lineptr = NULL;
	size_t n = 0;
	for (;;) {
		ssize_t r = getline(&lineptr, &n, stdin);
		if (r == -1) {
			if (errno == EINTR) { clearerr(stdin); continue; }
			free(lineptr);
			return NULL;
		}
		// Trim trailing newline
		if (r > 0 && lineptr[r - 1] == '\n') {
			lineptr[r - 1] = '\0';
		}
		break;
	}
	// Remove trailing CR if present (\r)
	size_t len = strlen(lineptr);
	if (len > 0 && lineptr[len - 1] == '\r') {
		lineptr[len - 1] = '\0';
	}
	return lineptr;
}




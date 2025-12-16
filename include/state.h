#ifndef STATE_H
#define STATE_H

#include <stddef.h>

void state_init(void);
const char *state_get_home(void);
const char *state_get_prev_cwd(void);
void state_set_prev_cwd(const char *path);

#endif



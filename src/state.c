#include "state.h"

#include <limits.h>
#include <string.h>
#include <unistd.h>

static char home_dir[PATH_MAX] = {0};
static char prev_cwd[PATH_MAX] = {0};

void state_init(void) {
	if (getcwd(home_dir, sizeof(home_dir)) == NULL) {
		strncpy(home_dir, "/", sizeof(home_dir) - 1);
		home_dir[sizeof(home_dir) - 1] = '\0';
	}
	prev_cwd[0] = '\0';
}

const char *state_get_home(void) {
	return home_dir;
}

const char *state_get_prev_cwd(void) {
	return prev_cwd;
}

void state_set_prev_cwd(const char *path) {
	if (!path) return;
	strncpy(prev_cwd, path, sizeof(prev_cwd) - 1);
	prev_cwd[sizeof(prev_cwd) - 1] = '\0';
}




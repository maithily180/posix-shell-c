#include "prompt.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pwd.h>
#include <limits.h>
#include <string.h>
#include "state.h"

void init_shell_home(void) {
	// Kept for API compatibility; actual home is tracked in state module
}

static void get_user_host(char *user, size_t user_sz, char *host, size_t host_sz) {
	user[0] = '\0';
	host[0] = '\0';

	struct passwd *pw = getpwuid(getuid());
	if (pw && pw->pw_name) {
		strncpy(user, pw->pw_name, user_sz - 1);
		user[user_sz - 1] = '\0';
	} else {
		strncpy(user, "user", user_sz - 1);
		user[user_sz - 1] = '\0';
	}

	if (gethostname(host, (int)host_sz) != 0) {
		strncpy(host, "host", host_sz - 1);
		host[host_sz - 1] = '\0';
	}
}

void show_prompt(void) {
	char cwd[PATH_MAX];
	if (!getcwd(cwd, sizeof(cwd))) {
		strncpy(cwd, "?", sizeof(cwd) - 1);
		cwd[sizeof(cwd) - 1] = '\0';
	}

	// Replace home prefix with ~ when appropriate
	char display_path[PATH_MAX + 2];
	const char *home = state_get_home();
	if (strncmp(cwd, home, strlen(home)) == 0) {
		const char *suffix = cwd + strlen(home);
		if (suffix[0] == '\0') {
			strncpy(display_path, "~", sizeof(display_path) - 1);
			display_path[sizeof(display_path) - 1] = '\0';
		} else if (suffix[0] == '/') {
			snprintf(display_path, sizeof(display_path), "~%s", suffix);
		} else {
			// shell_home not a true ancestor; show absolute
			strncpy(display_path, cwd, sizeof(display_path) - 1);
			display_path[sizeof(display_path) - 1] = '\0';
		}
	} else {
		strncpy(display_path, cwd, sizeof(display_path) - 1);
		display_path[sizeof(display_path) - 1] = '\0';
	}

	char user[128], host[128];
	get_user_host(user, sizeof(user), host, sizeof(host));

	printf("<%s@%s:%s> ", user, host, display_path);
	fflush(stdout);
}



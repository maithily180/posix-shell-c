#include "builtins.h"
#include "state.h"
#include "executor.h"
#include "jobs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <limits.h>
#include <signal.h>

static int compare_names(const void *a, const void *b) {
	const char *const *sa = (const char *const *)a;
	const char *const *sb = (const char *const *)b;
	return strcmp(*sa, *sb);
}

static int list_directory(const char *path, int flag_a, int flag_l) {
	DIR *d = opendir(path ? path : ".");
	if (!d) {
		printf("No such directory!\n");
		return 0;
	}
	char **names = NULL;
	size_t cap = 0, len = 0;
	struct dirent *ent;
	while ((ent = readdir(d)) != NULL) {
		if (!flag_a && ent->d_name[0] == '.') continue;
		if (len == cap) {
			size_t ncap = cap ? cap * 2 : 16;
			char **tmp = (char **)realloc(names, ncap * sizeof(char *));
			if (!tmp) break;
			names = tmp;
			cap = ncap;
		}
		names[len] = strdup(ent->d_name);
		if (names[len]) len++;
	}
	closedir(d);
	if (len == 0) {
		for (size_t i = 0; i < len; ++i) free(names[i]);
		free(names);
		if (!flag_l) printf("\n");
		return 0;
	}
	qsort(names, len, sizeof(char *), compare_names);
	if (flag_l) {
		for (size_t i = 0; i < len; ++i) {
			printf("%s\n", names[i]);
		}
	} else {
		for (size_t i = 0; i < len; ++i) {
			printf("%s%s", names[i], (i + 1 == len ? "\n" : " "));
		}
	}
	for (size_t i = 0; i < len; ++i) free(names[i]);
	free(names);
	return 0;
}

static int builtin_hop(int argc, char **argv) {
	char cwd[PATH_MAX];
	if (!getcwd(cwd, sizeof(cwd))) cwd[0] = '\0';

	if (argc == 1) {
		state_set_prev_cwd(cwd);
		return chdir(state_get_home());
	}
	for (int i = 1; i < argc; ++i) {
		const char *arg = argv[i];
		if (strcmp(arg, ".") == 0) {
			continue;
		} else if (strcmp(arg, "..") == 0) {
			state_set_prev_cwd(cwd);
			if (chdir("..") != 0) {
				// ignore
			}
			if (!getcwd(cwd, sizeof(cwd))) cwd[0] = '\0';
		} else if (strcmp(arg, "-") == 0) {
			const char *prev = state_get_prev_cwd();
			if (prev && prev[0] != '\0') {
				char tmp[PATH_MAX];
				strncpy(tmp, prev, sizeof(tmp) - 1);
				tmp[sizeof(tmp) - 1] = '\0';
				state_set_prev_cwd(cwd);
				chdir(tmp);
				if (!getcwd(cwd, sizeof(cwd))) cwd[0] = '\0';
			}
		} else if (strcmp(arg, "~") == 0) {
			state_set_prev_cwd(cwd);
			if (chdir(state_get_home()) != 0) {
				printf("No such directory!\n");
				return 0;
			}
			if (!getcwd(cwd, sizeof(cwd))) cwd[0] = '\0';
		} else {
			state_set_prev_cwd(cwd);
			if (chdir(arg) != 0) {
				printf("No such directory!\n");
				return 0;
			}
			if (!getcwd(cwd, sizeof(cwd))) cwd[0] = '\0';
		}
	}
	return 0;
}

static int builtin_reveal(int argc, char **argv) {
	int flag_a = 0, flag_l = 0;
	const char *path = NULL;
	int path_count = 0;
	
	for (int i = 1; i < argc; ++i) {
		const char *arg = argv[i];
		if (arg[0] == '-' && arg[1] != '\0') {
			for (int j = 1; arg[j] != '\0'; ++j) {
				if (arg[j] == 'a') flag_a = 1;
				else if (arg[j] == 'l') flag_l = 1;
			}
		} else {
			path = arg;
			path_count++;
		}
	}
	
	// Check for too many path arguments
	if (path_count > 1) {
		printf("reveal: Invalid Syntax!\n");
		return 0;
	}
	char target[PATH_MAX];
	if (!path || strcmp(path, ".") == 0) {
		if (!getcwd(target, sizeof(target))) {
			strncpy(target, ".", sizeof(target) - 1);
			target[sizeof(target) - 1] = '\0';
		}
	} else if (strcmp(path, "~") == 0) {
		strncpy(target, state_get_home(), sizeof(target) - 1);
		target[sizeof(target) - 1] = '\0';
	} else if (strcmp(path, "..") == 0) {
		if (!getcwd(target, sizeof(target))) {
			strncpy(target, ".", sizeof(target) - 1);
			target[sizeof(target) - 1] = '\0';
		}
		strncat(target, "/..", sizeof(target) - strlen(target) - 1);
	} else if (strcmp(path, "-") == 0) {
		const char *prev = state_get_prev_cwd();
		if (!prev || prev[0] == '\0') {
			printf("No such directory!\n");
			return 0;
		} else {
			strncpy(target, prev, sizeof(target) - 1);
			target[sizeof(target) - 1] = '\0';
		}
	} else {
		strncpy(target, path, sizeof(target) - 1);
		target[sizeof(target) - 1] = '\0';
	}
	return list_directory(target, flag_a, flag_l);
}

// history helpers
static void get_history_file(char *out, size_t out_sz) {
    const char *home = state_get_home();
    snprintf(out, out_sz, "%s/.mini_shell_history", home);
}

static void history_print(void) {
    char path[PATH_MAX];
    get_history_file(path, sizeof(path));
    FILE *f = fopen(path, "r");
    if (!f) return;
    char *buf = NULL; size_t n = 0; ssize_t r;
    while ((r = getline(&buf, &n, f)) != -1) {
        if (r > 0 && buf[r - 1] == '\n') buf[r - 1] = '\0';
        printf("%s\n", buf);
    }
    free(buf);
    fclose(f);
}

static void history_purge(void) {
    char path[PATH_MAX];
    get_history_file(path, sizeof(path));
    FILE *f = fopen(path, "w");
    if (f) fclose(f);
}

static int history_execute_index(int index) {
    if (index <= 0) return 0;
    // Load all lines first
    char path[PATH_MAX];
    get_history_file(path, sizeof(path));
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char *lines[16]; int count = 0;
    for (int i = 0; i < 16; ++i) lines[i] = NULL;
    char *buf = NULL; size_t n = 0; ssize_t r;
    while ((r = getline(&buf, &n, f)) != -1) {
        if (r > 0 && buf[r - 1] == '\n') buf[r - 1] = '\0';
        if (count < 15) lines[count++] = strdup(buf);
    }
    free(buf);
    fclose(f);
    // index is newest-to-oldest, 1-based
    if (count == 0) { return 0; }
    int idx = index - 1;
    if (idx < 0 || idx >= count) {
        for (int i = 0; i < count; ++i) free(lines[i]);
        return 0;
    }
    const char *cmd = lines[count - 1 - idx];
    if (cmd) {
        // Execute the entire stored shell_cmd without storing it again
        extern bool execute_shell_cmd(const char *input);
        execute_shell_cmd(cmd);
    }
    for (int i = 0; i < count; ++i) free(lines[i]);
    return 0;
}

static int builtin_log(int argc, char **argv) {
    if (argc == 1) {
        history_print();
        return 0;
    }
    if (argc == 2 && strcmp(argv[1], "purge") == 0) {
        history_purge();
        return 0;
    }
    if (argc == 3 && strcmp(argv[1], "execute") == 0) {
        int idx = atoi(argv[2]);
        return history_execute_index(idx);
    }
    return 0;
}

// Part E builtins

static int builtin_activities(int argc, char **argv) {
	if (argc != 1) {
		return 0; // Wrong number of arguments
	}
	jobs_list_activities();
	return 0;
}

static int builtin_ping(int argc, char **argv) {
	if (argc != 3) {
		printf("Invalid syntax!\n");
		return 0;
	}

	// Validate signal number is a valid integer
	const char *sig_str = argv[2];
	if (*sig_str == '\0') { printf("Invalid syntax!\n"); return 0; }
	for (const char *q = sig_str; *q; ++q) {
		if (*q < '0' || *q > '9') { printf("Invalid syntax!\n"); return 0; }
	}
	long sig_long = strtol(sig_str, NULL, 10);
	if (sig_long < 0) { printf("Invalid syntax!\n"); return 0; }
	int signal_num = (int)sig_long;
	int actual_signal = signal_num % 32;

	// Parse PID (accept decimal only). If invalid, treat as no such process.
	const char *pid_str = argv[1];
	if (*pid_str == '\0') { printf("No such process found\n"); return 0; }
	for (const char *q = pid_str; *q; ++q) {
		if (*q < '0' || *q > '9') { printf("No such process found\n"); return 0; }
	}
	long pid_long = strtol(pid_str, NULL, 10);
	if (pid_long <= 0) { printf("No such process found\n"); return 0; }
	pid_t pid = (pid_t)pid_long;

	if (kill(pid, actual_signal) == 0) {
		printf("Sent signal %d to process with pid %d\n", signal_num, (int)pid);
	} else {
		printf("No such process found\n");
	}
	return 0;
}

static int builtin_fg(int argc, char **argv) {
	int job_number = -1;
	
	if (argc == 1) {
		// No job number provided, use most recent
		job_number = jobs_get_most_recent_job();
		if (job_number == -1) {
			printf("No such job\n");
			return 0;
		}
	} else if (argc == 2) {
		job_number = atoi(argv[1]);
		if (job_number <= 0) {
			printf("No such job\n");
			return 0;
		}
	} else {
		printf("Invalid syntax!\n");
		return 0;
	}
	
	jobs_bring_to_foreground(job_number);
	return 0;
}

static int builtin_bg(int argc, char **argv) {
	int job_number = -1;
	
	if (argc == 1) {
		// No job number provided, use most recent
		job_number = jobs_get_most_recent_job();
		if (job_number == -1) {
			printf("No such job\n");
			return 0;
		}
	} else if (argc == 2) {
		job_number = atoi(argv[1]);
		if (job_number <= 0) {
			printf("No such job\n");
			return 0;
		}
	} else {
		printf("Invalid syntax!\n");
		return 0;
	}
	
	jobs_resume_background(job_number);
	return 0;
}

bool try_handle_builtin(char **argv, int argc) {
	if (argc <= 0 || !argv || !argv[0]) return false;
	if (strcmp(argv[0], "hop") == 0) {
		builtin_hop(argc, argv);
		return true;
	}
	if (strcmp(argv[0], "reveal") == 0) {
		builtin_reveal(argc, argv);
		return true;
	}
	if (strcmp(argv[0], "log") == 0) {
		builtin_log(argc, argv);
		return true;
	}
	// Part E builtins
	if (strcmp(argv[0], "activities") == 0) {
		builtin_activities(argc, argv);
		return true;
	}
	if (strcmp(argv[0], "ping") == 0) {
		builtin_ping(argc, argv);
		return true;
	}
	if (strcmp(argv[0], "fg") == 0) {
		builtin_fg(argc, argv);
		return true;
	}
	if (strcmp(argv[0], "bg") == 0) {
		builtin_bg(argc, argv);
		return true;
	}
	return false;
}



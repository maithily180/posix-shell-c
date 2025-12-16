#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <termios.h>

#include "prompt.h"
#include "input.h"
#include "parser.h"
#include "state.h"
#include "builtins.h"
#include "executor.h"
#include "history.h"
#include "jobs.h"

// Global variables for signal handling
pid_t foreground_pgid = 0;
static volatile sig_atomic_t sigchld_received = 0;

// Signal handler for Ctrl-C (SIGINT)
static void sigint_handler(int sig) {
	if (foreground_pgid > 0) {
		kill(-foreground_pgid, SIGINT);
	}
}

// Signal handler for Ctrl-Z (SIGTSTP)
static void sigtstp_handler(int sig) {
	if (foreground_pgid > 0) {
		kill(-foreground_pgid, SIGTSTP);
		// The job will be marked as stopped when we wait for it
		// Force a newline to ensure prompt appears on new line
		printf("\n");
		fflush(stdout);
	} else {
		// If no foreground process, just print newline and return to prompt
		printf("\n");
		fflush(stdout);
	}
	// Always return to prompt after Ctrl+Z
}

// Signal handler for SIGCHLD to report background job completion asynchronously
static void sigchld_handler(int sig) {
    (void)sig;
    sigchld_received = 1;
}

static void install_signal_handlers(void) {
	struct sigaction sa;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;

	sa.sa_handler = sigint_handler;
	sigaction(SIGINT, &sa, NULL);

	sa.sa_handler = sigtstp_handler;
	sigaction(SIGTSTP, &sa, NULL);

	// Handle child status changes for background job completion notifications
	sa.sa_handler = sigchld_handler;
	sigaction(SIGCHLD, &sa, NULL);

	// Prevent terminal background job signals from stopping the shell
	signal(SIGTTIN, SIG_IGN);
	signal(SIGTTOU, SIG_IGN);
}

int main(void) {
	install_signal_handlers();
	// Ensure shell has its own process group and does not get TSTP when idle
	setpgid(0, 0);
	tcsetpgrp(STDIN_FILENO, getpgrp());
	init_shell_home();
	state_init();
	jobs_init();

	for (;;) {
		// Check for completed background processes before showing prompt
		if (sigchld_received) { sigchld_received = 0; jobs_check_completed(); }
		
		show_prompt();
		// Check for completed jobs before reading input
		if (sigchld_received) { sigchld_received = 0; jobs_check_completed(); }
		char *line = read_line();
		if (!line) {
			// EOF: Ctrl-D behavior - kill all child processes and exit
			printf("logout\n");
			// Send SIGKILL to all known child processes
			jobs_kill_all();
			break;
		}

		// If any background jobs completed while waiting for input, report now
		if (sigchld_received) { sigchld_received = 0; jobs_check_completed(); }

		// Consume input per A.2: If invalid per grammar, print error.
		if (line[0] != '\0') {
			if (!parser_is_valid_command(line)) {
				printf("Invalid Syntax!\n");
			} else {
				// store history (Part B log)
				history_maybe_store(line);
				// Part D.1: enable sequential execution (;) with pipes/redirections
				// Part D.2: enable background execution (&)
				execute_shell_cmd(line);
				// Check for completed background jobs after command execution
				if (sigchld_received) { sigchld_received = 0; jobs_check_completed(); }
			}
		} else {
			// Even for empty commands, check for job completion
			if (sigchld_received) { sigchld_received = 0; jobs_check_completed(); }
		}

		free(line);
	}

	jobs_cleanup();
	return 0;
}



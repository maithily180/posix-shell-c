#include "executor.h"
#include "builtins.h"
#include "cmdparse.h"
#include "jobs.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <errno.h>
#include <signal.h>

// External reference to foreground process group
extern pid_t foreground_pgid;

static char *build_command_string(const CmdPipeline *group) {
    // Build a simple string like: "cmd1 arg1 | cmd2 arg2"
    size_t cap = 128;
    char *buf = (char *)malloc(cap);
    if (!buf) return NULL;
    buf[0] = '\0';
    size_t len = 0;
    for (int i = 0; i < group->count; ++i) {
        if (i > 0) {
            const char *sep = " | ";
            size_t sl = strlen(sep);
            if (len + sl + 1 > cap) { cap = (len + sl + 1) * 2; buf = (char *)realloc(buf, cap); if (!buf) return NULL; }
            memcpy(buf + len, sep, sl); len += sl; buf[len] = '\0';
        }
        char **argv = group->cmds[i].argv;
        for (int a = 0; argv && argv[a]; ++a) {
            if (a > 0) {
                if (len + 1 + 1 > cap) { cap = (len + 2) * 2; buf = (char *)realloc(buf, cap); if (!buf) return NULL; }
                buf[len++] = ' ';
                buf[len] = '\0';
            }
            const char *tok = argv[a];
            size_t tl = strlen(tok);
            if (len + tl + 1 > cap) { cap = (len + tl + 1) * 2; buf = (char *)realloc(buf, cap); if (!buf) return NULL; }
            memcpy(buf + len, tok, tl); len += tl; buf[len] = '\0';
        }
    }
    return buf;
}

static void skip_ws_idx(const char *s, size_t *i) {
	while (s[*i] == ' ' || s[*i] == '\t' || s[*i] == '\n' || s[*i] == '\r') (*i)++;
}

static char *parse_name_tok(const char *s, size_t *i) {
	skip_ws_idx(s, i);
	size_t start = *i;
	while (s[*i] != '\0') {
		char c = s[*i];
		if (c == '|' || c == '&' || c == '>' || c == '<' || c == ';' || c == '\n' || c == '\r' || c == '\t' || c == ' ') break;
		(*i)++;
	}
	if (*i == start) return NULL;
	size_t len = *i - start;
	char *out = (char *)malloc(len + 1);
	if (!out) return NULL;
	memcpy(out, s + start, len);
	out[len] = '\0';
	return out;
}

static int split_first_atomic(const char *input, char ***argv_out) {
	size_t i = 0;
	*argv_out = NULL;
	int argc = 0;

	// First atomic up to first pipe/; / &
	for (;;) {
		char *tok = parse_name_tok(input, &i);
		if (!tok) break;
		// Stop before encountering redirections; we only want argv (names)
		// If next non-ws is '|' ';' '&' '>' '<' then end collecting argv
		size_t save = i;
		skip_ws_idx(input, &i);
		char c = input[i];
		i = save;

		char **tmp = (char **)realloc(*argv_out, (argc + 2) * sizeof(char *));
		if (!tmp) { free(tok); break; }
		*argv_out = tmp;
		(*argv_out)[argc++] = tok;
		(*argv_out)[argc] = NULL;

		if (c == '|' || c == ';' || c == '&' || c == '>' || c == '<') {
			break;
		}
	}
	return argc;
}

bool execute_first_atomic(const char *input) {
	if (!input) return false;
	char **argv = NULL;
	int argc = split_first_atomic(input, &argv);
	if (argc <= 0) {
		free(argv);
		return false;
	}

	if (try_handle_builtin(argv, argc)) {
		for (int i = 0; i < argc; ++i) free(argv[i]);
		free(argv);
		return true;
	}

	pid_t pid = fork();
	if (pid == 0) {
		execvp(argv[0], argv);
		perror("execvp");
		exit(127);
	}
	if (pid > 0) {
		int status = 0;
		waitpid(pid, &status, 0);
	}

	for (int i = 0; i < argc; ++i) free(argv[i]);
	free(argv);
	return true;
}

static int setup_redirections(const Cmd *cmd) {
    // Handle input redirection (only the last one if multiple)
    if (cmd->in_file) {
        int fd = open(cmd->in_file, O_RDONLY);
        if (fd < 0) {
            fprintf(stderr, "No such file or directory\n");
            return -1;
        }
        if (dup2(fd, STDIN_FILENO) < 0) {
            close(fd);
            return -1;
        }
        close(fd);
    }
    
    // Handle output redirection (only the last one if multiple)
    if (cmd->out_file) {
        int flags = O_WRONLY | O_CREAT | (cmd->out_append ? O_APPEND : O_TRUNC);
        int fd = open(cmd->out_file, flags, 0666);
        if (fd < 0) {
            if (errno == EACCES || errno == EPERM) {
                fprintf(stderr, "Unable to create file for writing\n");
            }
            return -1;
        }
        if (dup2(fd, STDOUT_FILENO) < 0) {
            close(fd);
            return -1;
        }
        close(fd);
    }
    return 0;
}


bool execute_first_group_pipeline(const char *input) {
    CmdPipeline *pipep = parse_first_cmd_group(input);
    if (!pipep || pipep->count <= 0) {
        free_cmd_pipeline(pipep);
        return false;
    }

    // Single command without pipe: allow builtins
    if (pipep->count == 1) {
        Cmd *c = &pipep->cmds[0];
        int argc = 0; while (c->argv && c->argv[argc]) argc++;
        if (try_handle_builtin(c->argv, argc)) {
            // For builtins with redirection, we need to fork and handle redirection in child
            if (c->in_file || c->out_file) {
                pid_t pid = fork();
                if (pid == 0) {
                    // Child process
                    if (setup_redirections(c) != 0) {
                        _exit(1);
                    }
                    try_handle_builtin(c->argv, argc);
                    _exit(0);
                } else if (pid > 0) {
                    // Parent process
                    int status;
                    waitpid(pid, &status, 0);
                }
            }
            free_cmd_pipeline(pipep);
            return true;
        }
    }

    int n = pipep->count;
    int (*pipes)[2] = NULL;
    if (n > 1) {
        pipes = (int (*)[2])calloc((size_t)(n - 1), sizeof(int[2]));
        if (!pipes) { free_cmd_pipeline(pipep); return false; }
        for (int i = 0; i < n - 1; ++i) {
            if (pipe(pipes[i]) < 0) {
                // continue best-effort
            }
        }
    }

    pid_t *pids = (pid_t *)calloc((size_t)n, sizeof(pid_t));
    if (!pids) { free(pipes); free_cmd_pipeline(pipep); return false; }

    for (int i = 0; i < n; ++i) {
        pid_t pid = fork();
        if (pid == 0) {
            // child
            // connect pipes
            if (n > 1) {
                if (i > 0) {
                    dup2(pipes[i - 1][0], STDIN_FILENO);
                }
                if (i < n - 1) {
                    dup2(pipes[i][1], STDOUT_FILENO);
                }
                for (int k = 0; k < n - 1; ++k) {
                    close(pipes[k][0]);
                    close(pipes[k][1]);
                }
            }
            // redirections
            if (setup_redirections(&pipep->cmds[i]) != 0) {
                _exit(1);
            }
            
            // Check if this is a builtin command
            int argc = 0; while (pipep->cmds[i].argv && pipep->cmds[i].argv[argc]) argc++;
            if (try_handle_builtin(pipep->cmds[i].argv, argc)) {
                _exit(0); // Builtin executed successfully
            }
            
            execvp(pipep->cmds[i].argv[0], pipep->cmds[i].argv);
            // On failure, print exact spec-required message
            fprintf(stderr, "Command not found!\n");
            _exit(127);
        }
        pids[i] = pid;
    }

    if (n > 1) {
        for (int k = 0; k < n - 1; ++k) {
            close(pipes[k][0]);
            close(pipes[k][1]);
        }
    }

    for (int i = 0; i < n; ++i) {
        int status = 0;
        if (pids[i] > 0) waitpid(pids[i], &status, 0);
    }

    free(pids);
    free(pipes);
    free_cmd_pipeline(pipep);
    return true;
}

bool execute_shell_cmd(const char *input) {
    CmdSequence *seq = parse_shell_cmd(input);
    if (!seq || seq->count <= 0) {
        free_cmd_sequence(seq);
        return false;
    }

    // Execute each group sequentially
    for (int i = 0; i < seq->count; ++i) {
        CmdPipeline *group = &seq->groups[i];
        
        // Single command without pipe: allow builtins
        if (group->count == 1) {
            Cmd *c = &group->cmds[0];
            int argc = 0; while (c->argv && c->argv[argc]) argc++;
            if (try_handle_builtin(c->argv, argc)) {
                // For builtins with redirection, we need to fork and handle redirection in child
                if (c->in_file || c->out_file) {
                    pid_t pid = fork();
                    if (pid == 0) {
                        // Child process
                        if (setup_redirections(c) != 0) {
                            _exit(1);
                        }
                        try_handle_builtin(c->argv, argc);
                        _exit(0);
                    } else if (pid > 0) {
                        // Parent process
                        int status;
                        waitpid(pid, &status, 0);
                    }
                }
                continue; // builtin executed, move to next group
            }
        }

        // Execute as pipeline (handles both single commands and pipes)
        int n = group->count;
        int (*pipes)[2] = NULL;
        if (n > 1) {
            pipes = (int (*)[2])calloc((size_t)(n - 1), sizeof(int[2]));
            if (!pipes) continue; // skip this group on error
            for (int j = 0; j < n - 1; ++j) {
                if (pipe(pipes[j]) < 0) {
                    // continue best-effort
                }
            }
        }

        pid_t *pids = (pid_t *)calloc((size_t)n, sizeof(pid_t));
        if (!pids) { free(pipes); continue; }

        for (int j = 0; j < n; ++j) {
            pid_t pid = fork();
            if (pid == 0) {
                // child
                // Set process group for signal handling
                setpgid(0, 0);
                
                // connect pipes
                if (n > 1) {
                    if (j > 0) {
                        dup2(pipes[j - 1][0], STDIN_FILENO);
                    }
                    if (j < n - 1) {
                        dup2(pipes[j][1], STDOUT_FILENO);
                    }
                    for (int k = 0; k < n - 1; ++k) {
                        close(pipes[k][0]);
                        close(pipes[k][1]);
                    }
                }
                // redirections
                if (setup_redirections(&group->cmds[j]) != 0) {
                    _exit(1);
                }
                
                // Check if this is a builtin command
                int argc = 0; while (group->cmds[j].argv && group->cmds[j].argv[argc]) argc++;
                if (try_handle_builtin(group->cmds[j].argv, argc)) {
                    // Builtin executed successfully, exit with success
                    _exit(0);
                }
                
                execvp(group->cmds[j].argv[0], group->cmds[j].argv);
                // On failure, print exact spec-required message
                fprintf(stderr, "Command not found!\n");
                _exit(127);
            }
            pids[j] = pid;
            
            // Set process group for the first process in the pipeline
            if (j == 0) {
                setpgid(pid, pid);
                foreground_pgid = pid;
            } else {
                setpgid(pid, pids[0]);
            }
        }

        if (n > 1) {
            for (int k = 0; k < n - 1; ++k) {
                close(pipes[k][0]);
                close(pipes[k][1]);
            }
        }

        // Handle background vs foreground execution per-group based on parsed separator
        bool is_background_group = seq->groups[i].run_in_background;
        
        // For sequential execution, ensure each group completes before the next
        if (i > 0) {
            // Small delay to ensure file system operations complete
            // Use a lighter approach to ensure file system operations are visible
            fsync(STDOUT_FILENO);
            // Also ensure any pending I/O is flushed
            fflush(stdout);
            fflush(stderr);
            // Small delay to ensure file system operations are visible
            sleep(0); // Yield to scheduler
        }
        
        if (is_background_group) {
            // Background execution: don't wait, add to job tracking
            // For simplicity, we'll track the first process in the pipeline
            if (n > 0 && pids[0] > 0) {
                // Create a command string for job tracking (entire pipeline)
                char *cmd_str = build_command_string(group);
                if (!cmd_str) cmd_str = strdup("unknown");
                // Add " &" to the command string for background jobs
                char *bg_cmd = malloc(strlen(cmd_str) + 3);
                strcpy(bg_cmd, cmd_str);
                strcat(bg_cmd, " &");
                int job_num = jobs_add(pids[0], bg_cmd, true);
                if (job_num > 0) {
                    jobs_print_job(job_num, pids[0]);
                }
                free(cmd_str);
                free(bg_cmd);
            }
        } else {
            // Foreground execution: give terminal to job's process group, then wait
            if (n > 0 && pids[0] > 0) {
                // Transfer terminal control to the foreground job's process group
                tcsetpgrp(STDIN_FILENO, pids[0]);
            }
            // Foreground execution: wait for all processes in this group to complete or stop
            for (int j = 0; j < n; ++j) {
                int status = 0;
                if (pids[j] > 0) {
                    pid_t result;
                    for (;;) {
                        result = waitpid(pids[j], &status, WUNTRACED);
                        if (result == -1 && errno == EINTR) { continue; }
                        break;
                    }
                    if (result > 0 && WIFSTOPPED(status)) {
                        // Process was stopped (Ctrl-Z)
                        const char *cmd_name = group->cmds[j].argv[0] ? group->cmds[j].argv[0] : "unknown";
                        // Add to job tracking as stopped first to get proper job number (store full pipeline)
                        char *cmd_str = build_command_string(group);
                        if (!cmd_str) cmd_str = strdup(cmd_name);
                        int job_num = jobs_add(pids[0], cmd_str, false);
                        jobs_set_stopped(job_num);
                        free(cmd_str);
                        if (job_num > 0) {
                            printf("[%d] Stopped %s\n", job_num, cmd_name);
                            fflush(stdout);
                        }
                    }
                }
            }
            // Restore terminal control back to the shell
            tcsetpgrp(STDIN_FILENO, getpgrp());
            // Clear foreground process group after the pipeline finishes or stops
            foreground_pgid = 0;
        }

        free(pids);
        free(pipes);
    }

    free_cmd_sequence(seq);
    return true;
}



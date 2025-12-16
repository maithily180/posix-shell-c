#include "cmdparse.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

typedef struct {
	const char *s;
	size_t i;
} P;

static void skip_ws(P *p) {
	while (p->s[p->i] == ' ' || p->s[p->i] == '\t' || p->s[p->i] == '\n' || p->s[p->i] == '\r') p->i++;
}

static char *parse_name(P *p) {
	skip_ws(p);
	size_t start = p->i;
	while (p->s[p->i] != '\0') {
		char c = p->s[p->i];
		if (c == '|' || c == '&' || c == '>' || c == '<' || c == ';' || c == '\n' || c == '\r' || c == '\t' || c == ' ') break;
		p->i++;
	}
	if (p->i == start) return NULL;
	size_t len = p->i - start;
	char *out = (char *)malloc(len + 1);
	if (!out) return NULL;
	memcpy(out, p->s + start, len);
	out[len] = '\0';
	return out;
}

static int append_argv(char ***argv, int *argc, char *tok) {
	char **tmp = (char **)realloc(*argv, (size_t)(*argc + 2) * sizeof(char *));
	if (!tmp) return -1;
	*argv = tmp;
	(*argv)[*argc] = tok;
	(*argc)++;
	(*argv)[*argc] = NULL;
	return 0;
}

static int parse_atomic(P *p, Cmd *cmd) {
	char *name = parse_name(p);
	if (!name) return -1;
	int argc = 0; char **argv = NULL;
	if (append_argv(&argv, &argc, name) != 0) return -1;

	for (;;) {
		size_t save = p->i;
		char *tok = parse_name(p);
		if (tok) {
			if (append_argv(&argv, &argc, tok) != 0) { free(tok); break; }
			continue;
		}
		p->i = save;
		skip_ws(p);
		if (p->s[p->i] == '<') {
			p->i++;
			char *in = parse_name(p);
			if (!in) { // invalid, but builder only fills until now
				break;
			}
			// For multiple input redirects, check if the file exists
			// If it doesn't exist, keep this as the error file
			if (!cmd->in_file) {
				cmd->in_file = in;
			} else {
				// Check if previous file exists, if not, keep it for error
				FILE *test = fopen(cmd->in_file, "r");
				if (test) {
					fclose(test);
					// Previous file exists, use new one
					free(cmd->in_file);
					cmd->in_file = in;
				} else {
					// Previous file doesn't exist, keep it for error
					free(in);
				}
			}
			continue;
		}
		if (p->s[p->i] == '>') {
			p->i++;
			int append = 0;
			if (p->s[p->i] == '>') { append = 1; p->i++; }
			char *out = parse_name(p);
			if (!out) { break; }
			// For multiple output redirects, check if the file can be created
			// If it can't be created, keep this as the error file
			if (!cmd->out_file) {
				cmd->out_file = out;
				cmd->out_append = append;
			} else {
				// Check if previous file can be created, if not, keep it for error
				int flags = O_WRONLY | O_CREAT | (cmd->out_append ? O_APPEND : O_TRUNC);
				int fd = open(cmd->out_file, flags, 0666);
				if (fd >= 0) {
					close(fd);
					// Previous file can be created, use new one
					free(cmd->out_file);
					cmd->out_file = out;
					cmd->out_append = append;
				} else {
					// Previous file can't be created, keep it for error
					free(out);
				}
			}
			continue;
		}
		break;
	}
	cmd->argv = argv;
	return 0;
}

static CmdPipeline *parse_first_cmd_group_from_pos(P *p) {
	CmdPipeline *cp = (CmdPipeline *)calloc(1, sizeof(CmdPipeline));
	if (!cp) return NULL;

	for (;;) {
		Cmd cmd; memset(&cmd, 0, sizeof(cmd));
		if (parse_atomic(p, &cmd) != 0) { free_cmd_pipeline(cp); return NULL; }
		Cmd *tmp = (Cmd *)realloc(cp->cmds, (size_t)(cp->count + 1) * sizeof(Cmd));
		if (!tmp) { free_cmd_pipeline(cp); return NULL; }
		cp->cmds = tmp;
		cp->cmds[cp->count++] = cmd;

		size_t save = p->i;
		skip_ws(p);
		if (p->s[p->i] == '|') {
			p->i++;
			continue;
		}
		p->i = save;
		break;
	}
	return cp;
}

CmdPipeline *parse_first_cmd_group(const char *input) {
	if (!input) return NULL;
	P p = { input, 0 };
	return parse_first_cmd_group_from_pos(&p);
}

void free_cmd_pipeline(CmdPipeline *p) {
	if (!p) return;
	for (int i = 0; i < p->count; ++i) {
		Cmd *c = &p->cmds[i];
		if (c->argv) {
			for (int j = 0; c->argv[j]; ++j) free(c->argv[j]);
			free(c->argv);
		}
		free(c->in_file);
		free(c->out_file);
	}
	free(p->cmds);
	free(p);
}

CmdSequence *parse_shell_cmd(const char *input) {
	if (!input) return NULL;
	P p = { input, 0 };
	CmdSequence *seq = (CmdSequence *)calloc(1, sizeof(CmdSequence));
	if (!seq) return NULL;
	seq->is_background = false; // Initialize trailing background flag

	for (;;) {
		CmdPipeline *group = parse_first_cmd_group_from_pos(&p);
		if (!group) { free_cmd_sequence(seq); return NULL; }
		
		CmdPipeline *tmp = (CmdPipeline *)realloc(seq->groups, (size_t)(seq->count + 1) * sizeof(CmdPipeline));
		if (!tmp) { free_cmd_pipeline(group); free_cmd_sequence(seq); return NULL; }
		seq->groups = tmp;
		// Default each group's mode to foreground; will adjust based on separator seen after it
		group->run_in_background = false;
		seq->groups[seq->count++] = *group;
		free(group); // only free the container, not the contents

		// After a group, consume any '&' (mark previous as background),
		// and a single ';' to indicate there is another group to parse.
		// This allows patterns like "cmd & ; next".
		bool has_semicolon = false;
		for (;;) {
			skip_ws(&p);
			if (p.s[p.i] == '&') {
				seq->groups[seq->count - 1].run_in_background = true;
				p.i++;
				continue;
			}
			if (p.s[p.i] == ';') {
				p.i++;
				has_semicolon = true;
			}
			break;
		}
		if (has_semicolon) {
			continue;
		}
		break;
	}

	// Optional trailing &
	skip_ws(&p);
	if (p.s[p.i] == '&') {
		p.i++;
		seq->is_background = true;
		// Mark the last group to run in background as well
		if (seq->count > 0) {
			seq->groups[seq->count - 1].run_in_background = true;
		}
	}

	// must consume all input (ignoring whitespace)
	skip_ws(&p);
	if (p.s[p.i] != '\0') { free_cmd_sequence(seq); return NULL; }

	return seq;
}

void free_cmd_sequence(CmdSequence *s) {
	if (!s) return;
	for (int i = 0; i < s->count; ++i) {
		// Free the contents of each CmdPipeline (not the structure itself)
		CmdPipeline *group = &s->groups[i];
		for (int j = 0; j < group->count; ++j) {
			Cmd *c = &group->cmds[j];
			if (c->argv) {
				for (int k = 0; c->argv[k]; ++k) free(c->argv[k]);
				free(c->argv);
			}
			free(c->in_file);
			free(c->out_file);
		}
		free(group->cmds);
	}
	free(s->groups);
	free(s);
}






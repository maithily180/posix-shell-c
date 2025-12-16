#ifndef CMDPARSE_H
#define CMDPARSE_H

#include <stdbool.h>

typedef struct {
	char **argv;      // NULL-terminated
	char *in_file;    // optional, malloc'd, may be NULL
	char *out_file;   // optional, malloc'd, may be NULL
	int out_append;   // 0 for trunc, 1 for append
} Cmd;

typedef struct {
	Cmd *cmds; // array of commands in a pipeline
	int count; // number of commands
    bool run_in_background; // whether this group should run in background
} CmdPipeline;

typedef struct {
	CmdPipeline *groups; // array of command groups (separated by ; or &)
	int count; // number of groups
	bool is_background; // true if command ends with & (legacy trailing)
} CmdSequence;

// Parse only the first cmd_group from input. Returns NULL on failure.
CmdPipeline *parse_first_cmd_group(const char *input);

// Parse entire shell_cmd into sequence of cmd_groups. Returns NULL on failure.
CmdSequence *parse_shell_cmd(const char *input);

void free_cmd_pipeline(CmdPipeline *p);
void free_cmd_sequence(CmdSequence *s);

#endif





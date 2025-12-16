#include "history.h"
#include "builtins.h"
#include "state.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

static void get_history_file(char *out, size_t out_sz) {
	const char *home = state_get_home();
	snprintf(out, out_sz, "%s/.mini_shell_history", home);
}

void history_maybe_store(const char *line) {
	if (!line || line[0] == '\0') return;
	// Don't store if any atomic command name is 'log'. Scan the input for command names.
	const char *p = line;
	int expecting_cmd_name = 1;
	while (*p) {
		while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
		if (*p == '\0') break;
		if (*p == '|' || *p == ';' || *p == '&') { expecting_cmd_name = 1; p++; continue; }
		if (*p == '>' || *p == '<') {
			if (*p == '>' && *(p+1) == '>') p++;
			p++;
			while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
			while (*p && *p!='|' && *p!='&' && *p!=';' && *p!='>' && *p!='<' && *p!=' ' && *p!='\t' && *p!='\n' && *p!='\r') p++;
			continue;
		}
		const char *start = p;
		while (*p && *p!='|' && *p!='&' && *p!=';' && *p!='>' && *p!='<' && *p!=' ' && *p!='\t' && *p!='\n' && *p!='\r') p++;
		if (expecting_cmd_name) {
			size_t len = (size_t)(p - start);
			if (len == 3 && strncmp(start, "log", 3) == 0) return;
			expecting_cmd_name = 0;
		}
	}

	char path[PATH_MAX];
	get_history_file(path, sizeof(path));
	FILE *f = fopen(path, "r");
	char *last = NULL; ssize_t r;
	char *buf = NULL; size_t nb = 0;
	int count = 0;
	if (f) {
		while ((r = getline(&buf, &nb, f)) != -1) {
			if (r > 0 && buf[r - 1] == '\n') buf[r - 1] = '\0';
			free(last);
			last = strdup(buf);
			count++;
		}
		free(buf);
		fclose(f);
	}
	if (last && strcmp(last, line) == 0) {
		free(last);
		return;
	}
	free(last);


	// Load all and append, keeping max 15
	char *lines[16];
	for (int i = 0; i < 16; ++i) lines[i] = NULL;
	count = 0;
	f = fopen(path, "r");
	buf = NULL; nb = 0;
	if (f) {
		while ((r = getline(&buf, &nb, f)) != -1) {
			if (r > 0 && buf[r - 1] == '\n') buf[r - 1] = '\0';
			if (count < 15) lines[count++] = strdup(buf);
		}
		free(buf);
		fclose(f);
	}
	if (count == 15) {
		free(lines[0]);
		for (int i = 1; i < 15; ++i) lines[i - 1] = lines[i];
		count = 14;
	}
	lines[count++] = strdup(line);

	f = fopen(path, "w");
	if (f) {
		for (int i = 0; i < count; ++i) {
			fprintf(f, "%s\n", lines[i]);
			free(lines[i]);
		}
		fclose(f);
	}
}




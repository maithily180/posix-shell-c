#include "parser.h"

#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

// A small recursive-descent style validator for the provided grammar.
// Grammar (whitespace can appear between tokens):
// shell_cmd  ->  cmd_group ((& | ;) cmd_group)* &?
// cmd_group  ->  atomic (| atomic)*
// atomic     ->  name (name | input | output)*
// input      ->  < name | <name
// output     ->  > name | >name | >> name | >>name
// name       ->  r"[^|&><;]+"

typedef struct {
	const char *s;
	size_t i;
} Parser;

static void skip_ws(Parser *p) {
	while (p->s[p->i] == ' ' || p->s[p->i] == '\t' || p->s[p->i] == '\n' || p->s[p->i] == '\r') {
		p->i++;
	}
}

static bool parse_name(Parser *p) {
	skip_ws(p);
	size_t start = p->i;
	while (p->s[p->i] != '\0') {
		char c = p->s[p->i];
		if (c == '|' || c == '&' || c == '>' || c == '<' || c == ';' || c == '\n' || c == '\r' || c == '\t' || c == ' ') {
			break;
		}
		p->i++;
	}
	return p->i > start;
}

static bool parse_input(Parser *p) {
	skip_ws(p);
	if (p->s[p->i] != '<') return false;
	p->i++;
	// optional whitespace already handled by parse_name
	return parse_name(p);
}

static bool parse_output(Parser *p) {
	skip_ws(p);
	if (p->s[p->i] != '>') return false;
	p->i++;
	if (p->s[p->i] == '>') {
		p->i++; // >>
	}
	return parse_name(p);
}

static bool parse_atomic(Parser *p) {
	if (!parse_name(p)) return false; // command name
	for (;;) {
		size_t save = p->i;
		if (parse_name(p)) {
			continue;
		}
		p->i = save;
		if (parse_input(p)) {
			continue;
		}
		p->i = save;
		if (parse_output(p)) {
			continue;
		}
		p->i = save;
		break;
	}
	return true;
}

static bool parse_cmd_group(Parser *p) {
	if (!parse_atomic(p)) return false;
	for (;;) {
		size_t save = p->i;
		skip_ws(p);
		if (p->s[p->i] == '|') {
			p->i++;
			if (!parse_atomic(p)) return false;
			continue;
		}
		p->i = save;
		break;
	}
	return true;
}

static bool parse_shell_cmd(Parser *p) {
	if (!parse_cmd_group(p)) return false;
	for (;;) {
		size_t save = p->i;
		// Allow one or more '&' between groups (for background marker), then an optional ';'
		for (;;) {
			skip_ws(p);
			if (p->s[p->i] == '&') { p->i++; continue; }
			break;
		}
		skip_ws(p);
		if (p->s[p->i] == ';') {
			p->i++;
			if (!parse_cmd_group(p)) return false;
			continue;
		}
		p->i = save;
		break;
	}

	// Optional trailing &
	skip_ws(p);
	while (p->s[p->i] == '&') { p->i++; }

	// must consume all input (ignoring whitespace)
	skip_ws(p);
	return p->s[p->i] == '\0';
}

bool parser_is_valid_command(const char *input) {
	if (!input) return false;
	Parser p = { input, 0 };
	return parse_shell_cmd(&p);
}




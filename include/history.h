#ifndef HISTORY_H
#define HISTORY_H

// Persist up to 15 commands, skip consecutive duplicates and skip commands where any atomic is 'log'
void history_maybe_store(const char *line);

#endif



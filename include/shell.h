#ifndef SHELL_H
#define SHELL_H

// Current working directory — updated by cmd_cd, read by builtins
extern char current_path[256];

void shell_run();

#endif

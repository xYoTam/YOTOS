#ifndef BUILTINS_H
#define BUILTINS_H

#include <stdint.h>

typedef struct {
    const char *name;
    void (*func)(char *arg);
} command_t;


void cmd_help(char *arg);
void cmd_clear(char *arg);
void cmd_mem(char *arg);
void cmd_ls(char *arg);
void cmd_cat(char *arg);
void cmd_cd(char *arg);
void cmd_touch(char *arg);
void cmd_mkdir(char *arg);
void cmd_echo(char *arg);
void cmd_exec(char *arg);


extern command_t commands[];
extern const uint32_t command_count;

#endif

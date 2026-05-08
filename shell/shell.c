#include <stdint.h>
#include "builtins.h"
#include "shell.h"
#include "keyboard.h"
#include "uart.h"
#include "stdio.h"
#include "vga.h"
#include "string.h"
#include "pmm.h"

#define INPUT_SIZE 256

char current_path[256] = "/";


void draw_banner() {
    change_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    printf("  --------------------------------\n");
    printf("            WELCOME TO\n");
    change_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    printf("  __  __  ___  ____  ____  ___  \n");
    printf(" \\ \\/ / / _ \\|_  _||  _ \\/ __| \n");
    printf("  \\  / | (_) | | |  | (_) \\__ \\ \n");
    printf("  /_/   \\___/  |_|  |___/|___/ \n");
    change_color(VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
    printf("  --------------------------------\n");
    change_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    printf("\n");
}


/*
 * shell_execute - parse one input line and dispatch to the matching command.
 *
 * Splits on the first space: everything before is the command name, everything
 * after (with leading spaces stripped) is passed as `arg` to the handler.
 * If there is no space, arg is an empty string "".
 */
static void shell_execute(char *input) {
    // Find the first space and split the line in-place
    char *cmd = input;
    char *arg = "";

    for (int i = 0; input[i]; i++) {
        if (input[i] == ' ') {
            input[i] = '\0';        // terminate the command name
            arg = &input[i + 1];   // arg starts right after the space

            // Skip any additional leading spaces in the argument
            while (*arg == ' ') arg++;
            break;
        }
    }

    for (uint32_t i = 0; i < command_count; i++) {
        if (strcmp(cmd, commands[i].name) == 0) {
            commands[i].func(arg);
            return;
        }
    }

    if (strcmp(cmd, "") != 0) {
        printf("unknown command: %s\n", cmd);
    }
}



void shell_run() {
    char input[INPUT_SIZE];
    int i = 0;
    int from_uart = 0;

    terminal_initialize();

    draw_banner();
    printf("YOTOS:%s> ", current_path);

    while (1) {
        char c = keyboard_getchar();
        if (c) {
            from_uart = 0;
        } else {
            c = uart_getchar();
            if (c) from_uart = 1;
        }
        if (!c) continue;

        if (c == '\n') {
            terminal_putchar('\n');
            input[i] = '\0';
            uart_output_mode = from_uart;
            shell_execute(input);
            uart_output_mode = 0;
            i = 0;
            printf("YOTOS:%s> ", current_path);
        }
        else if (c == '\b') {
            if (i > 0) {
                i--;
                terminal_putchar('\b');
            }
        }
        else if (i < INPUT_SIZE - 1) {
            terminal_putchar(c);
            input[i++] = c;
        }
    }
}

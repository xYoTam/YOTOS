#include <stdint.h>
#include "builtins.h"
#include "shell.h"
#include "pmm.h"
#include "vga.h"
#include "stdio.h"
#include "vfs.h"
#include "string.h"
#include "kheap.h"
#include "process.h"
#include "keyboard.h"


command_t commands[] = {
    {"help",  cmd_help},
    {"clear", cmd_clear},
    {"mem",   cmd_mem},
    {"ls",    cmd_ls},
    {"cat",   cmd_cat},
    {"cd",    cmd_cd},
    {"touch", cmd_touch},
    {"mkdir", cmd_mkdir},
    {"echo",  cmd_echo},
    {"exec",  cmd_exec},
};

const uint32_t command_count = sizeof(commands) / sizeof(commands[0]);


// ---------------------------------------
//          PATH HELPERS
// ---------------------------------------

/*
 * path_resolve - build an absolute path from current_path and arg.
 *
 * Rules:
 *   arg is NULL or ""  → copy current_path into out
 *   arg starts with /  → already absolute, copy into out
 *   otherwise          → append arg to current_path with a / separator
 */
static void path_resolve(const char *arg, char *out, uint32_t size) {
    if (!arg || !arg[0]) {
        strlcpy(out, current_path, size);
        return;
    }
    if (arg[0] == '/') {
        strlcpy(out, arg, size);
        return;
    }
    // Relative path — prepend current_path
    strlcpy(out, current_path, size);
    uint32_t len = strlen(out);
    // Add separator if not already at root "/"
    if (len > 0 && out[len - 1] != '/' && len < size - 1) {
        out[len++] = '/';
        out[len]   = '\0';
    }
    strlcpy(out + len, arg, size - len);
}


/*
 * path_parent_and_name - split a resolved absolute path into parent dir and
 * base name.
 *
 * e.g. "/subdir/file.txt" → parent="/subdir"  base="file.txt"
 *      "/file.txt"        → parent="/"        base="file.txt"
 */
static void path_parent_and_name(const char *resolved,
                                  char *parent, uint32_t psz,
                                  char *base,   uint32_t bsz) {
    // Find the last '/'
    int last = 0;
    for (int i = 0; resolved[i]; i++)
        if (resolved[i] == '/') last = i;

    if (last == 0) {
        // e.g. "/file.txt"
        strlcpy(parent, "/", psz);
        strlcpy(base, resolved + 1, bsz);
    } else {
        // Copy everything up to (not including) the last slash
        uint32_t i;
        for (i = 0; i < (uint32_t)last && i < psz - 1; i++)
            parent[i] = resolved[i];
        parent[i] = '\0';
        strlcpy(base, resolved + last + 1, bsz);
    }
}


// ---------------------------------------
// 			BUILTIN COMMANDS:
// ---------------------------------------

void cmd_help(char *arg) {
    (void)arg;
    printf("YOTOS ---===--- YOTAM'S OS\ncommands:\nhelp, clear, mem, ls, cat, cd, touch, mkdir, echo, exec\n      ---===---\n");
}

void cmd_clear(char *arg) {
    (void)arg;
    terminal_initialize();
}

void cmd_mem(char *arg) {
    (void)arg;
    printf("free:  %d frames\n", pmmngr_get_free_block_count());
    printf("used:  %d frames\n", pmmngr_get_used_blocks());
    printf("total: %d frames\n", pmmngr_get_max_blocks());
}

void cmd_ls(char *arg) {
    char path[256];
    path_resolve(arg, path, sizeof(path));

    vfs_node_t *dir = vfs_open(path);
    if (!dir) {
        printf("ls: %s: not found\n", path);
        return;
    }
    if (!(dir->flags & VFS_DIR)) {
        printf("ls: %s: not a directory\n", path);
        vfs_close(dir);
        return;
    }

    uint32_t i = 0;
    vfs_dirent_t *entry;
    while ((entry = vfs_readdir(dir, i++)) != 0) {
        if (entry->flags & VFS_DIR)
            printf("[DIR]  %s\n", entry->name);
        else
            printf("[FILE] %s\n", entry->name);
    }

    vfs_close(dir);
}

void cmd_cat(char *arg) {
    if (!arg || arg[0] == '\0') {
        printf("cat: missing filename\n");
        return;
    }

    char path[256];
    path_resolve(arg, path, sizeof(path));

    vfs_node_t *file = vfs_open(path);
    if (!file) {
        printf("cat: %s: not found\n", path);
        return;
    }
    if (!(file->flags & VFS_FILE)) {
        printf("cat: %s: is a directory\n", path);
        vfs_close(file);
        return;
    }

    uint8_t buf[512];
    uint32_t offset = 0;
    int bytes;

    while ((bytes = vfs_read(file, offset, sizeof(buf), buf)) > 0) {
        for (int i = 0; i < bytes; i++)
            putchar((char)buf[i]);
        offset += bytes;
    }

    printf("\n");
    vfs_close(file);
}

void cmd_cd(char *arg) {
    // cd with no argument or "/" goes to root
    if (!arg || !arg[0] || strcmp(arg, "/") == 0) {
        strlcpy(current_path, "/", sizeof(current_path));
        return;
    }

    // cd .. — go up one level
    if (strcmp(arg, "..") == 0) {
        if (current_path[0] == '/' && current_path[1] == '\0')
            return; // already at root, nowhere to go
        // Find the last '/' and truncate there
        int last = 0;
        for (int i = 0; current_path[i]; i++)
            if (current_path[i] == '/') last = i;
        if (last == 0)
            current_path[1] = '\0'; // "/something" → "/"
        else
            current_path[last] = '\0'; // "/a/b" → "/a"
        return;
    }

    // Resolve the target path and verify it exists and is a directory
    char resolved[256];
    path_resolve(arg, resolved, sizeof(resolved));

    vfs_node_t *node = vfs_open(resolved);
    if (!node) {
        printf("cd: %s: not found\n", resolved);
        return;
    }
    if (!(node->flags & VFS_DIR)) {
        printf("cd: %s: not a directory\n", resolved);
        vfs_close(node);
        return;
    }
    vfs_close(node);

    strlcpy(current_path, resolved, sizeof(current_path));
}

void cmd_touch(char *arg) {
    if (!arg || !arg[0]) { printf("touch: missing filename\n"); return; }

    char resolved[256], parent[256], base[128];
    path_resolve(arg, resolved, sizeof(resolved));
    path_parent_and_name(resolved, parent, sizeof(parent), base, sizeof(base));

    if (!base[0]) { printf("touch: invalid path\n"); return; }

    vfs_node_t *dir = vfs_open(parent);
    if (!dir) { printf("touch: %s: not found\n", parent); return; }
    if (!(dir->flags & VFS_DIR)) {
        printf("touch: %s: not a directory\n", parent);
        vfs_close(dir);
        return;
    }

    vfs_node_t *node = vfs_create(dir, base, VFS_FILE);
    if (!node) printf("touch: failed to create %s\n", base);
    else       vfs_close(node);

    vfs_close(dir);
}

void cmd_mkdir(char *arg) {
    if (!arg || !arg[0]) { printf("mkdir: missing name\n"); return; }

    char resolved[256], parent[256], base[128];
    path_resolve(arg, resolved, sizeof(resolved));
    path_parent_and_name(resolved, parent, sizeof(parent), base, sizeof(base));

    if (!base[0]) { printf("mkdir: invalid path\n"); return; }

    vfs_node_t *dir = vfs_open(parent);
    if (!dir) { printf("mkdir: %s: not found\n", parent); return; }
    if (!(dir->flags & VFS_DIR)) {
        printf("mkdir: %s: not a directory\n", parent);
        vfs_close(dir);
        return;
    }

    vfs_node_t *node = vfs_create(dir, base, VFS_DIR);
    if (!node) printf("mkdir: failed to create %s\n", base);
    else       vfs_close(node);

    vfs_close(dir);
}

void cmd_echo(char *arg) {
    if (!arg || !arg[0]) { printf("\n"); return; }

    // Find '>' for output redirection
    char *gt = 0;
    for (int i = 0; arg[i]; i++) {
        if (arg[i] == '>') {
					gt = &arg[i];
					break;
				}
    }

    if (!gt) {
        // No redirection — print to screen
        printf("%s\n", arg);
        return;
    }

    // Split: terminate content at '>', advance past spaces to filename
    *gt = '\0';
    char *filename = gt + 1;
    while (*filename == ' ') filename++;

    // Strip trailing spaces from content
    int end = (int)strlen(arg) - 1;
    while (end >= 0 && arg[end] == ' ') arg[end--] = '\0';

    if (!filename[0]) { printf("echo: missing filename after >\n"); return; }

    char resolved[256], parent[256], base[128];
    path_resolve(filename, resolved, sizeof(resolved));

    // Open existing file, or create it if it doesn't exist
    vfs_node_t *file = vfs_open(resolved);
    if (!file) {
        path_parent_and_name(resolved, parent, sizeof(parent), base, sizeof(base));
        vfs_node_t *dir = vfs_open(parent);
        if (!dir) { printf("echo: %s: not found\n", parent); return; }
        file = vfs_create(dir, base, VFS_FILE);
        vfs_close(dir);
        if (!file) { printf("echo: failed to create %s\n", filename); return; }
    }

    if (!(file->flags & VFS_FILE)) {
        printf("echo: %s: is a directory\n", filename);
        vfs_close(file);
        return;
    }

    uint32_t len = (uint32_t)strlen(arg);
    vfs_write(file, 0, len, (uint8_t *)arg);
    uint8_t nl = '\n';
    vfs_write(file, len, 1, &nl);

    vfs_close(file);
}

void cmd_exec(char *arg) {
    if (!arg || !arg[0]) { printf("exec: missing filename\n"); return; }

    char path[256];
    path_resolve(arg, path, sizeof(path));

    vfs_node_t *file = vfs_open(path);
    if (!file) { printf("exec: %s: not found\n", path); return; }
    if (!(file->flags & VFS_FILE)) {
        printf("exec: %s: not a file\n", path);
        vfs_close(file);
        return;
    }

    process_t *proc = process_create(file);
    vfs_close(file);

    if (!proc) { printf("exec: failed to load %s\n", path); return; }

    keyboard_flush();
    process_run(proc);
    printf("[exec] returned\n");
    terminal_initialize();
    printf("[exec] exited with code %d\n", proc->exit_code);
    process_free(proc);
}

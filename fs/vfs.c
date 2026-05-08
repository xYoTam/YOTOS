#include "vfs.h"
#include "stdio.h"
#include "string.h"
#include <stdint.h>

// Mount table

#define VFS_MAX_MOUNTS 8

typedef struct {
    char        path[128];   // mount point path, e.g. "/"
    vfs_node_t *root;        // root node of the mounted filesystem
} vfs_mountpoint_t;

static vfs_mountpoint_t mounts[VFS_MAX_MOUNTS];
static int mount_count = 0;

// Mount

void vfs_mount(const char *path, vfs_node_t *root) {
    if (!path || !root) return;
    if (mount_count >= VFS_MAX_MOUNTS) {
        printf("[VFS] Mount table full\n");
        return;
    }
    strlcpy(mounts[mount_count].path, path, 128);
    mounts[mount_count].root = root;
    mount_count++;
    printf("[VFS] Mounted %s at %s\n", root->name, path);
}

vfs_node_t *vfs_get_root() {
    for (int i = 0; i < mount_count; i++) {
        const char *p = mounts[i].path;
        if (p[0] == '/' && p[1] == '\0')
            return mounts[i].root;
    }
    return 0;
}



// Path resolution

/*
 * vfs_open resolves a path against the root mount, traversing subdirectories.
 *
 * Accepted forms:
 *   "README.TXT"         (bare name       → look up in root directory)
 *   "/README.TXT"        (absolute        → look up in root directory)
 *   "/subdir/file.txt"   (subdirectory    → traverse each component)
 *   "/"                  (root itself     → return root node directly,
 *                                           do NOT call vfs_close on it)
 *
 * Intermediate directory nodes allocated by finddir are closed automatically.
 * Only the final returned node must be released by the caller with vfs_close().
 */
vfs_node_t *vfs_open(const char *name) {
    vfs_node_t *root = vfs_get_root();
    if (!root) return 0;

    // Strip a leading slash
    if (name[0] == '/') name++;

    // Bare "/" means the caller wants the root directory itself
    if (name[0] == '\0') return root;

    // Walk the path one component at a time
    vfs_node_t *current = root;
    const char *p = name;

    while (*p) {
        // Extract the next path component into a local buffer
        char component[128];
        int i = 0;
        while (*p && *p != '/' && i < 127)
            component[i++] = *p++;
        component[i] = '\0';

        // Skip the slash separator
        if (*p == '/') p++;

        // Look up this component inside the current directory
        vfs_node_t *next = vfs_finddir(current, component);

        // Free the intermediate node (but never free the static root)
        if (current != root) vfs_close(current);

        if (!next) return 0;
        current = next;
    }

    return current;
}


// Dispatch functions

void vfs_close(vfs_node_t *node) {
    if (!node) return;
    if (node->ops && node->ops->close)
        node->ops->close(node);
    // If close is NULL the node is statically allocated — don't free it.
}

int vfs_read(vfs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buf) {
    if (!node) return -1;
    if (!(node->flags & VFS_FILE)) return -1;
    if (!node->ops || !node->ops->read) return -1;
    return node->ops->read(node, offset, size, buf);
}

int vfs_write(vfs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buf) {
    if (!node) return -1;
    if (!(node->flags & VFS_FILE)) return -1;
    if (!node->ops || !node->ops->write) return -1;
    return node->ops->write(node, offset, size, buf);
}

vfs_dirent_t *vfs_readdir(vfs_node_t *node, uint32_t index) {
    if (!node) return 0;
    if (!(node->flags & VFS_DIR)) return 0;
    if (!node->ops || !node->ops->readdir) return 0;
    return node->ops->readdir(node, index);
}

vfs_node_t *vfs_finddir(vfs_node_t *node, const char *name) {
    if (!node) return 0;
    if (!(node->flags & VFS_DIR)) return 0;
    if (!node->ops || !node->ops->finddir) return 0;
    return node->ops->finddir(node, name);
}

vfs_node_t *vfs_create(vfs_node_t *node, const char *name, uint32_t flags) {
    if (!node) return 0;
    if (!(node->flags & VFS_DIR)) return 0;
    if (!node->ops || !node->ops->create) return 0;
    return node->ops->create(node, name, flags);
}

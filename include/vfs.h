#ifndef VFS_H
#define VFS_H

#include <stdint.h>

// Node types flags
#define VFS_FILE  0x01
#define VFS_DIR   0x02

typedef struct vfs_node    vfs_node_t;
typedef struct vfs_dirent  vfs_dirent_t;
typedef struct vfs_ops     vfs_ops_t;

/*
 * vfs_ops_t  -  the dispatch table every filesystem must fill in.
 *
 * A filesystem registers its root vfs_node_t (via vfs_mount) and populates
 * one or both ops tables — one for directory nodes, one for file nodes.
 *
 *   read    - copy `size` bytes starting at `offset` into `buf`.
 *             Returns bytes actually read, or -1 on error.
 *             Only meaningful on VFS_FILE nodes.
 *
 *   readdir - return the `index`-th valid entry in a directory node.
 *             Returns NULL when the index is past the last entry.
 *             Points to a static buffer reused on each call — callers must
 *             consume the result before the next call.
 *             Only meaningful on VFS_DIR nodes.
 *
 *   finddir - look up a child node by name inside a directory node.
 *             Returns a heap-allocated vfs_node_t *, or NULL if not found.
 *             The caller must free it with vfs_close().
 *             Only meaningful on VFS_DIR nodes.
 *
 *   close   - release any resources owned by the node (e.g. kfree it).
 *             Set to NULL for statically-allocated nodes (e.g. the root).
 */
struct vfs_ops {
    int           (*read)   (vfs_node_t *node, uint32_t offset,
                              uint32_t size, uint8_t *buf);
    int           (*write)  (vfs_node_t *node, uint32_t offset,
                              uint32_t size, uint8_t *buf);
    vfs_dirent_t *(*readdir)(vfs_node_t *node, uint32_t index);
    vfs_node_t   *(*finddir)(vfs_node_t *node, const char *name);
    vfs_node_t   *(*create) (vfs_node_t *node, const char *name,
                              uint32_t flags);
    void          (*close)  (vfs_node_t *node);
};

/*
 * vfs_node_t  -  one file or directory in the virtual filesystem tree.
 *
 *   name  - the node's own name (basename, not a full path)
 *   flags - VFS_FILE or VFS_DIR
 *   size  - file size in bytes (0 for directories)
 *   inode - filesystem-specific identifier (for FAT16: first data cluster)
 *   ops   - pointer to the dispatch table for this node type
 */
struct vfs_node {
    char       name[128];
    uint32_t   flags;
    uint32_t   size;
    uint32_t   inode;
    vfs_ops_t *ops;
    uint32_t   dir_lba;    // sector that holds this node's directory entry
    uint32_t   dir_index;  // which of the 16 entries in that sector
};

/*
 * vfs_dirent_t  -  a lightweight directory listing entry.
 *
 * Returned by vfs_readdir(); the caller must not free it.
 * Contents are valid only until the next vfs_readdir() call on the same node.
 */
struct vfs_dirent {
    char     name[128];
    uint32_t inode;
    uint32_t flags;   // VFS_FILE or VFS_DIR
};

// Public API

/*
 * vfs_mount - attach a filesystem root node at `path`.
 *
 * Only absolute paths are accepted.  Use "/" to mount the root filesystem.
 * Example:
 *   vfs_mount("/", fat16_vfs_root());
 */
void vfs_mount(const char *path, vfs_node_t *root);

/*
 * vfs_get_root - return the root node of the "/" mount, or NULL if nothing
 * is mounted there yet.  Useful for shell commands like `ls`.
 */
vfs_node_t *vfs_get_root();

/*
 * vfs_open - look up a file by name and return an open node.
 *
 * `name` may be a bare filename ("README.TXT") or an absolute path
 * ("/README.TXT").  Subdirectory traversal is not yet implemented.
 * Returns NULL if the file does not exist or no filesystem is mounted.
 * The returned node must be released with vfs_close().
 */
vfs_node_t *vfs_open(const char *name);

/*
 * vfs_close - release a node previously returned by vfs_open / vfs_finddir.
 *
 * Calls the filesystem's close callback if one is registered (some nodes
 * are statically allocated and must not be freed).
 */
void vfs_close(vfs_node_t *node);

/*
 * vfs_read - read bytes from an open file node.
 * Returns the number of bytes read, or -1 on error.
 */
int vfs_read (vfs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buf);
int vfs_write(vfs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buf);

/*
 * vfs_readdir - iterate a directory node.
 * Returns the entry at position `index`, or NULL past the last entry.
 * The returned pointer is valid until the next vfs_readdir() call.
 */
vfs_dirent_t *vfs_readdir(vfs_node_t *node, uint32_t index);

/*
 * vfs_finddir - look up a child node by name inside a directory.
 * Returns a heap-allocated node, or NULL.  Must be closed with vfs_close().
 */
vfs_node_t *vfs_finddir(vfs_node_t *node, const char *name);

/*
 * vfs_create - create a new file or directory inside a directory node.
 *
 * `flags` should be VFS_FILE or VFS_DIR.
 * Returns a heap-allocated node for the new entry, or NULL on failure.
 * The caller must release it with vfs_close().
 */
vfs_node_t *vfs_create(vfs_node_t *node, const char *name, uint32_t flags);

#endif

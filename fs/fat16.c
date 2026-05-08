#include "fat16.h"
#include "vfs.h"
#include "ata.h"
#include "kheap.h"
#include "stdio.h"
#include "string.h"
#include <stdint.h>

static fat16_bpb_t bpb;
static uint32_t fat_lba;          // LBA of first FAT sector
static uint32_t root_dir_lba;     // LBA of first root-directory sector
static uint32_t root_dir_sectors; // number of sectors in the root directory
static uint32_t data_lba;         // LBA of cluster 2 (first data cluster)
static int      mounted = 0;


static void read_sector(uint32_t lba, uint8_t *buf) {
    ata_read_sector(lba, buf);
}

static void write_sector(uint32_t lba, uint8_t *buf) {
    ata_write_sector(lba, buf);
}

// Forward declaration — defined later after dir_sector_lba
static uint32_t dir_sector_lba(vfs_node_t *node, uint32_t n);


/*
 * fat_entry - return the FAT16 table entry for `cluster`.
 *
 * Each FAT16 entry is 2 bytes, so the byte offset inside the FAT is
 * cluster * 2.  We read the one 512-byte sector that contains it.
 */
static uint16_t fat_entry(uint16_t cluster) {
    uint8_t  buf[512];
    uint32_t byte_off    = (uint32_t)cluster * 2;
    uint32_t fat_sector  = fat_lba + byte_off / 512;
    uint32_t within_sect = byte_off % 512;
    read_sector(fat_sector, buf);
    return *(uint16_t *)(buf + within_sect);
}


/*
 * fat_set_entry - write a FAT16 entry for `cluster` with `value`.
 * Updates both FAT copies if the volume has two.
 */
static void fat_set_entry(uint16_t cluster, uint16_t value) {
    uint8_t  buf[512];
    uint32_t byte_off    = (uint32_t)cluster * 2;
    uint32_t fat_sector  = fat_lba + byte_off / 512;
    uint32_t within_sect = byte_off % 512;

    read_sector(fat_sector, buf);
    *(uint16_t *)(buf + within_sect) = value;
    write_sector(fat_sector, buf);

    // Mirror to second FAT copy if present
    if (bpb.fat_count > 1) {
        uint32_t fat2_sector = fat_lba + bpb.fat_size_16 + byte_off / 512;
        read_sector(fat2_sector, buf);
        *(uint16_t *)(buf + within_sect) = value;
        write_sector(fat2_sector, buf);
    }
}

/*
 * fat_alloc_cluster - find the first free cluster (FAT entry == 0),
 * mark it as end-of-chain (0xFFFF), and return its index.
 * Returns 0 if the disk is full.
 */
static uint16_t fat_alloc_cluster() {
    uint8_t buf[512];

    for (uint32_t s = 0; s < bpb.fat_size_16; s++) {
        read_sector(fat_lba + s, buf);
        uint16_t *entries = (uint16_t *)buf;

        for (int i = 0; i < 256; i++) {
            uint32_t cluster = s * 256 + i;
            if (cluster < 2) continue;   // clusters 0 and 1 are reserved
            if (entries[i] == 0x0000) {
                fat_set_entry((uint16_t)cluster, 0xFFFF);
                return (uint16_t)cluster;
            }
        }
    }
    return 0;   // disk full
}

/*
 * fat_free_chain - free all clusters in the chain starting at `cluster`.
 */
static void fat_free_chain(uint16_t cluster) {
    while (cluster >= 2 && cluster < FAT16_CLUSTER_EOF_LO) {
        uint16_t next = fat_entry(cluster);
        fat_set_entry(cluster, 0x0000);
        cluster = next;
    }
}


/*
 * read_cluster - read all sectors of `cluster` into `buf`.
 * buf must be at least (sectors_per_cluster × 512) bytes.
 */
static void read_cluster(uint16_t cluster, uint8_t *buf) {
    uint32_t lba = data_lba + (uint32_t)(cluster - 2) * bpb.sectors_per_cluster;
    for (uint8_t i = 0; i < bpb.sectors_per_cluster; i++)
        read_sector(lba + i, buf + (uint32_t)i * 512);
}



/*
 * format_83 - render the space-padded 8+3 fields as "NAME.EXT\0".
 * dst must be at least 13 bytes.
 */
static void format_83(const fat16_direntry_t *e, char *dst) {
    int j = 0;

    int last = 7;
    while (last >= 0 && e->name[last] == ' ')
        last--;
    for (int k = 0; k <= last; k++)
        dst[j++] = e->name[k];

    if (e->ext[0] != ' ') {
        dst[j++] = '.';
        int ext_last = 2;
        while (ext_last >= 0 && e->ext[ext_last] == ' ')
            ext_last--;
        for (int k = 0; k <= ext_last; k++)
            dst[j++] = e->ext[k];
    }

    dst[j] = '\0';
}


/*
 * format_name_83 - convert a normal filename into the space-padded 8.3
 * fields used by FAT16 directory entries.
 * Returns 0 on success, -1 if the name doesn't fit 8.3 rules.
 */
static int format_name_83(const char *name, uint8_t out_name[8], uint8_t out_ext[3]) {
    for (int i = 0; i < 8; i++) out_name[i] = ' ';
    for (int i = 0; i < 3; i++) out_ext[i]  = ' ';

    // Find the dot separating name from extension
    int dot = -1;
    int len = 0;
    while (name[len]) len++;
    for (int i = 0; i < len; i++)
        if (name[i] == '.') { dot = i; break; }

    int name_len = (dot >= 0) ? dot : len;
    int ext_len  = (dot >= 0) ? len - dot - 1 : 0;

    if (name_len == 0 || name_len > 8) return -1;
    if (ext_len > 3) return -1;

    for (int i = 0; i < name_len; i++) {
        char c = name[i];
        if (c >= 'a' && c <= 'z') c -= 32;
        out_name[i] = (uint8_t)c;
    }
    for (int i = 0; i < ext_len; i++) {
        char c = name[dot + 1 + i];
        if (c >= 'a' && c <= 'z') c -= 32;
        out_ext[i] = (uint8_t)c;
    }
    return 0;
}

/*
 * fat16_write_dir_entry - find a free slot in `dir_node` and write a new
 * directory entry with the given fields.
 * Returns 0 on success, -1 if no free slot exists or name is invalid.
 */
static int fat16_write_dir_entry(vfs_node_t *dir_node, const char *name,
                                  uint16_t first_cluster, uint32_t file_size,
                                  uint8_t attributes) {
    uint8_t name83[8], ext83[3];
    if (format_name_83(name, name83, ext83) < 0) return -1;

    uint8_t sector[512];

    for (uint32_t s = 0; ; s++) {
        uint32_t lba = dir_sector_lba(dir_node, s);
        if (!lba) return -1;   // no free slot found

        read_sector(lba, sector);
        fat16_direntry_t *entries = (fat16_direntry_t *)sector;

        for (int i = 0; i < 16; i++) {
            uint8_t first = entries[i].name[0];
            if (first == 0x00 || first == 0xE5) {
                // Free slot — fill it in
                for (int j = 0; j < 8; j++) entries[i].name[j] = name83[j];
                for (int j = 0; j < 3; j++) entries[i].ext[j]  = ext83[j];
                entries[i].attributes    = attributes;
                for (int j = 0; j < 10; j++) entries[i].reserved[j] = 0;
                entries[i].write_time    = 0;
                entries[i].write_date    = 0;
                entries[i].first_cluster = first_cluster;
                entries[i].file_size     = file_size;
                write_sector(lba, sector);
                return 0;
            }
        }
    }
}


static int name_match(const fat16_direntry_t *e, const char *name) {
    char buf[13];
    format_83(e, buf);
    return strcasecmp(buf, name) == 0;
}


// Returns 1 → skip this entry, 2 → end-of-directory, 0 → valid entry
static int skip_entry(const fat16_direntry_t *e) {
    if (e->name[0] == 0x00)
        return 2;
    if ((uint8_t)e->name[0] == 0xE5)
        return 1;   // deleted
    if (e->attributes == FAT16_ATTR_LFN)
        return 1; // long-filename piece

    return 0;
}



// VFS operation callbacks for FAT16

/*
 * fat16_vfs_read - read `size` bytes from file `node` starting at `offset`.
 *
 * Uses node->inode as the starting cluster and node->size as the file length.
 * Handles arbitrary offsets by skipping whole clusters before reading.
 */
static int fat16_vfs_read(vfs_node_t *node, uint32_t offset,
                           uint32_t size, uint8_t *buf) {
    if (!mounted)
        return -1;

    uint32_t file_size = node->size;
    if (offset >= file_size)
        return 0;

    // Cap size to what's actually available
    uint32_t max_read = file_size - offset;
    if (size > max_read) size = max_read;

    uint32_t cluster_bytes = (uint32_t)bpb.sectors_per_cluster * 512;
    uint16_t cluster       = (uint16_t)node->inode;

    // Skip whole clusters to reach the one that contains `offset`
    uint32_t skip_count  = offset / cluster_bytes;
    uint32_t inner_off   = offset % cluster_bytes;

    for (uint32_t i = 0; i < skip_count; i++) {
        if (cluster < 2 || cluster >= FAT16_CLUSTER_EOF_LO)
            return 0;
        cluster = fat_entry(cluster);
    }

    uint8_t *cbuf = (uint8_t *)kmalloc(cluster_bytes);
    if (!cbuf) return -1;

    uint32_t bytes_read = 0;
    int      first      = 1;

    while (bytes_read < size && cluster >= 2 && cluster < FAT16_CLUSTER_EOF_LO) {
        read_cluster(cluster, cbuf);

        // On the first cluster we may start partway through it
        uint32_t start = first ? inner_off : 0;
        uint32_t avail = cluster_bytes - start;
        uint32_t copy  = (bytes_read + avail > size) ? (size - bytes_read) : avail;

        for (uint32_t i = 0; i < copy; i++)
            buf[bytes_read + i] = cbuf[start + i];

        bytes_read += copy;
        cluster = fat_entry(cluster);
        first   = 0;
    }

    kfree(cbuf);
    return (int)bytes_read;
}


/*
 * dir_sector_lba - return the LBA of the nth sector inside a directory node.
 *
 * For the root directory (inode == 0): sectors are contiguous at root_dir_lba.
 * For subdirectories: sectors live in the data area, reached by following the
 * cluster chain in the FAT.
 * Returns 0 if n is past the end of the directory.
 */
static uint32_t dir_sector_lba(vfs_node_t *node, uint32_t n) {
    if (node->inode == 0) {
        // Root directory — fixed contiguous region
        if (n >= root_dir_sectors) return 0;
        return root_dir_lba + n;
    }

    // Cluster-based subdirectory — walk the FAT chain
    uint32_t spc         = bpb.sectors_per_cluster;
    uint32_t cluster_idx = n / spc;      // which cluster in the chain
    uint32_t sect_in_cl  = n % spc;      // which sector inside that cluster

    uint16_t cluster = (uint16_t)node->inode;
    for (uint32_t i = 0; i < cluster_idx; i++) {
        if (cluster < 2 || cluster >= FAT16_CLUSTER_EOF_LO)
            return 0;
        cluster = fat_entry(cluster);
    }
    if (cluster < 2 || cluster >= FAT16_CLUSTER_EOF_LO)
        return 0;

    return data_lba + (uint32_t)(cluster - 2) * spc + sect_in_cl;
}


/*
 * fat16_vfs_readdir - return the `index`-th valid entry in a directory node.
 *
 * Works for both the root directory and cluster-based subdirectories via
 * dir_sector_lba().  Skips deleted entries, LFN pieces, volume labels, and
 * the . / .. dot entries.
 * Returns a pointer to a static buffer reused on every call.
 */
static vfs_dirent_t fat16_dirent_buf;   // static, reused each call

static vfs_dirent_t *fat16_vfs_readdir(vfs_node_t *node, uint32_t index) {
    if (!mounted) return 0;

    uint8_t  sector[512];
    uint32_t valid = 0;

    for (uint32_t s = 0; ; s++) {
        uint32_t lba = dir_sector_lba(node, s);
        if (!lba) return 0;   // past end of directory

        read_sector(lba, sector);
        fat16_direntry_t *entries = (fat16_direntry_t *)sector;

        for (int i = 0; i < 16; i++) {
            fat16_direntry_t *e = &entries[i];
            int sk = skip_entry(e);
            if (sk == 2) return 0;   // end-of-directory marker
            if (sk == 1) continue;
            if (e->attributes & FAT16_ATTR_VOLUME_ID) continue;
            if (e->name[0] == '.') continue;  // skip . and ..

            if (valid == index) {
                format_83(e, fat16_dirent_buf.name);
                fat16_dirent_buf.inode = e->first_cluster;
                fat16_dirent_buf.flags = (e->attributes & FAT16_ATTR_DIRECTORY)
                                          ? VFS_DIR : VFS_FILE;
                return &fat16_dirent_buf;
            }
            valid++;
        }
    }
}



/*
 * fat16_vfs_finddir - search a directory node for an entry named `name`.
 *
 * Works for both the root directory and cluster-based subdirectories via
 * dir_sector_lba().  Returns a heap-allocated vfs_node_t for both files and
 * subdirectories (caller must vfs_close it), or NULL if not found.
 */

// Forward declarations so finddir/create can reference both ops tables
static void fat16_vfs_close(vfs_node_t *node);
static vfs_ops_t fat16_file_ops;
static vfs_ops_t fat16_dir_ops;
static vfs_node_t *fat16_vfs_create(vfs_node_t *node, const char *name, uint32_t flags);

static vfs_node_t *fat16_vfs_finddir(vfs_node_t *node, const char *name) {
    if (!mounted) return 0;

    uint8_t sector[512];

    for (uint32_t s = 0; ; s++) {
        uint32_t lba = dir_sector_lba(node, s);
        if (!lba) return 0;

        read_sector(lba, sector);
        fat16_direntry_t *entries = (fat16_direntry_t *)sector;

        for (int i = 0; i < 16; i++) {
            fat16_direntry_t *e = &entries[i];
            int sk = skip_entry(e);
            if (sk == 2) return 0;
            if (sk == 1) continue;
            if (e->attributes & FAT16_ATTR_VOLUME_ID) continue;

            if (name_match(e, name)) {
                vfs_node_t *n = (vfs_node_t *)kmalloc(sizeof(vfs_node_t));
                if (!n) return 0;
                format_83(e, n->name);
                n->inode     = e->first_cluster;
                n->dir_lba   = lba;
                n->dir_index = (uint32_t)i;

                if (e->attributes & FAT16_ATTR_DIRECTORY) {
                    n->flags = VFS_DIR;
                    n->size  = 0;
                    n->ops   = &fat16_dir_ops;
                } else {
                    n->flags = VFS_FILE;
                    n->size  = e->file_size;
                    n->ops   = &fat16_file_ops;
                }
                return n;
            }
        }
    }
}



static void fat16_vfs_close(vfs_node_t *node) {
    // File and subdir nodes are heap-allocated by finddir/create — free them.
    kfree(node);
}


/*
 * fat16_vfs_create - create a new file or subdirectory inside `node`.
 *
 * For VFS_DIR: allocates a cluster, zeroes it, writes . and .. entries,
 *              then writes the directory entry in the parent.
 * For VFS_FILE: writes a directory entry with first_cluster=0 and size=0.
 *
 * Returns a heap-allocated vfs_node_t for the new entry (caller must
 * vfs_close it), or NULL on failure.
 */
static vfs_node_t *fat16_vfs_create(vfs_node_t *node, const char *name,
                                     uint32_t flags) {
    if (!mounted) return 0;

    if (flags & VFS_DIR) {
        // Allocate a cluster for the new directory's contents
        uint16_t cluster = fat_alloc_cluster();
        if (!cluster) { printf("[FAT16] Disk full\n"); return 0; }

        // Zero the entire cluster so stale data doesn't appear as entries
        uint8_t zero[512];
        for (int i = 0; i < 512; i++) zero[i] = 0;
        uint32_t spc = bpb.sectors_per_cluster;
        uint32_t lba = data_lba + (uint32_t)(cluster - 2) * spc;
        for (uint32_t s = 0; s < spc; s++)
            write_sector(lba + s, zero);

        // Write . and .. entries in the first sector of the new cluster
        uint8_t sector[512];
        for (int i = 0; i < 512; i++) sector[i] = 0;
        fat16_direntry_t *entries = (fat16_direntry_t *)sector;

        // . — points to this directory
        for (int i = 0; i < 8; i++) entries[0].name[i] = ' ';
        for (int i = 0; i < 3; i++) entries[0].ext[i]  = ' ';
        entries[0].name[0]       = '.';
        entries[0].attributes    = FAT16_ATTR_DIRECTORY;
        entries[0].first_cluster = cluster;
        entries[0].file_size     = 0;

        // .. — points to the parent directory
        for (int i = 0; i < 8; i++) entries[1].name[i] = ' ';
        for (int i = 0; i < 3; i++) entries[1].ext[i]  = ' ';
        entries[1].name[0]       = '.';
        entries[1].name[1]       = '.';
        entries[1].attributes    = FAT16_ATTR_DIRECTORY;
        entries[1].first_cluster = (uint16_t)node->inode;
        entries[1].file_size     = 0;

        write_sector(lba, sector);

        // Write the entry in the parent directory
        if (fat16_write_dir_entry(node, name, cluster, 0,
                                   FAT16_ATTR_DIRECTORY) < 0) {
            fat_free_chain(cluster);
            return 0;
        }
    } else {
        // Empty file — no cluster needed yet
        if (fat16_write_dir_entry(node, name, 0, 0, FAT16_ATTR_ARCHIVE) < 0)
            return 0;
    }

    return fat16_vfs_finddir(node, name);
}




/*
 * fat16_vfs_write - write `size` bytes from `buf` into `node` at `offset`.
 *
 * Allocates clusters on demand as the file grows.  After writing, updates
 * the directory entry on disk (first_cluster and file_size) using the
 * node's dir_lba / dir_index fields set by finddir or create.
 */
static int fat16_vfs_write(vfs_node_t *node, uint32_t offset,
                            uint32_t size, uint8_t *buf) {
    if (!mounted || size == 0) return 0;

    uint32_t cluster_bytes = (uint32_t)bpb.sectors_per_cluster * 512;

    // Allocate a first cluster if the file is empty
    if ((uint16_t)node->inode < 2) {
        uint16_t c = fat_alloc_cluster();
        if (!c) return -1;
        node->inode = c;
    }

    uint8_t *cbuf = (uint8_t *)kmalloc(cluster_bytes);
    if (!cbuf) return -1;

    uint32_t bytes_written = 0;
    int ok = 1;

    while (bytes_written < size && ok) {
        uint32_t cur_off     = offset + bytes_written;
        uint32_t cluster_idx = cur_off / cluster_bytes;
        uint32_t inner_off   = cur_off % cluster_bytes;

        // Walk the FAT chain to cluster_idx, extending it if needed
        uint16_t cluster = (uint16_t)node->inode;
        for (uint32_t i = 0; i < cluster_idx; i++) {
            uint16_t next = fat_entry(cluster);
            if (next < 2 || next >= FAT16_CLUSTER_EOF_LO) {
                uint16_t nc = fat_alloc_cluster();
                if (!nc) { ok = 0; break; }
                fat_set_entry(cluster, nc);
                next = nc;
            }
            cluster = next;
        }
        if (!ok) break;

        // Read-modify-write: patch new data into the existing cluster content
        read_cluster(cluster, cbuf);

        uint32_t avail = cluster_bytes - inner_off;
        uint32_t copy  = size - bytes_written;
        if (copy > avail) copy = avail;

        for (uint32_t i = 0; i < copy; i++)
            cbuf[inner_off + i] = buf[bytes_written + i];

        uint32_t lba = data_lba + (uint32_t)(cluster - 2) * bpb.sectors_per_cluster;
        for (uint32_t s = 0; s < bpb.sectors_per_cluster; s++)
            write_sector(lba + s, cbuf + s * 512);

        bytes_written += copy;
    }

    kfree(cbuf);

    // Update size and first_cluster in the on-disk directory entry
    uint32_t new_size = offset + bytes_written;
    if (new_size > node->size) node->size = new_size;

    uint8_t dsec[512];
    read_sector(node->dir_lba, dsec);
    fat16_direntry_t *de = (fat16_direntry_t *)dsec;
    de[node->dir_index].first_cluster = (uint16_t)node->inode;
    de[node->dir_index].file_size     = node->size;
    write_sector(node->dir_lba, dsec);

    return (int)bytes_written;
}


// Ops tables

// Ops for file nodes returned by finddir
static vfs_ops_t fat16_file_ops = {
    .read    = fat16_vfs_read,
    .write   = fat16_vfs_write,
    .readdir = 0,
    .finddir = 0,
    .create  = 0,
    .close   = fat16_vfs_close,
};

// Ops for subdirectory nodes returned by finddir (heap-allocated)
static vfs_ops_t fat16_dir_ops = {
    .read    = 0,
    .write   = 0,
    .readdir = fat16_vfs_readdir,
    .finddir = fat16_vfs_finddir,
    .create  = fat16_vfs_create,
    .close   = fat16_vfs_close,
};

// Ops for the root directory node (statically allocated, never freed)
static vfs_ops_t fat16_root_ops = {
    .read    = 0,
    .write   = 0,
    .readdir = fat16_vfs_readdir,
    .finddir = fat16_vfs_finddir,
    .create  = fat16_vfs_create,
    .close   = 0,
};

// The root directory node — statically allocated, lives for the kernel's lifetime
static vfs_node_t fat16_root_node = {
    .name  = "fat16-root",
    .flags = VFS_DIR,
    .size  = 0,
    .inode = 0,
    .ops   = &fat16_root_ops,
};




// Public API

/*
 * fat16_init - read the boot sector, validate it, and compute layout constants.
 *
 * Disk layout:
 *
 *   Sector 0                              Boot Sector (BPB)
 *   Sectors 1 … reserved-1               Reserved
 *   Sectors reserved … +fat_count×fat_sz FAT copies (fat_count of them)
 *   Sectors after FATs                   Root directory (fixed-size)
 *   Sectors after root dir               Data area — cluster 2 starts here
 */
int fat16_init() {
    uint8_t sector[512];
    read_sector(0, sector);

    if (sector[510] != 0x55 || sector[511] != 0xAA) {
        printf("[FAT16] Bad boot sector signature\n");
        return -1;
    }

    bpb = *(fat16_bpb_t *)sector;

    if (bpb.bytes_per_sector != 512) {
        printf("[FAT16] Unsupported sector size: %d\n", bpb.bytes_per_sector);
        return -1;
    }
    if (bpb.fat_size_16 == 0) {
        printf("[FAT16] FAT32 not supported\n");
        return -1;
    }

    fat_lba          = bpb.reserved_sectors;
    root_dir_lba     = fat_lba + (uint32_t)bpb.fat_count * bpb.fat_size_16;
    root_dir_sectors = ((uint32_t)bpb.root_entry_count * 32 + 511) / 512;
    data_lba         = root_dir_lba + root_dir_sectors;
    mounted          = 1;

    char label[12];
    for (int i = 0; i < 11; i++) label[i] = (char)bpb.volume_label[i];
    label[11] = '\0';

    printf("[FAT16] Mounted: %s  fat@%d root@%d data@%d\n",
           label, fat_lba, root_dir_lba, data_lba);
    return 0;
}



/*
 * fat16_vfs_root - return the static VFS node for the FAT16 root directory.
 *
 * Pass this to vfs_mount("/", fat16_vfs_root()) after fat16_init() succeeds.
 * Do NOT call vfs_close() on the returned pointer.
 */
vfs_node_t *fat16_vfs_root() {
    return &fat16_root_node;
}

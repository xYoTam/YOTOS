#ifndef FAT16_H
#define FAT16_H

#include <stdint.h>
#include "vfs.h"

// Directory entry attribute bits
#define FAT16_ATTR_READ_ONLY  0x01
#define FAT16_ATTR_HIDDEN     0x02
#define FAT16_ATTR_SYSTEM     0x04
#define FAT16_ATTR_VOLUME_ID  0x08
#define FAT16_ATTR_DIRECTORY  0x10
#define FAT16_ATTR_ARCHIVE    0x20
#define FAT16_ATTR_LFN        0x0F   // all four low bits set = long filename entry

// FAT16 end-of-chain sentinel
#define FAT16_CLUSTER_EOF_LO  0xFFF8  // any value >= this is end-of-chain



//BIOS Parameter Block (bytes 0-61 of the boot sector)
typedef struct __attribute__((packed)) {
    uint8_t  jump_boot[3];        // 0x00  EB xx 90 or E9 xx xx
    uint8_t  oem_name[8];         // 0x03  e.g. "mkdosfs "

    uint16_t bytes_per_sector;    // 0x0B  almost always 512
    uint8_t  sectors_per_cluster; // 0x0D  1/2/4/8/16/32/64/128
    uint16_t reserved_sectors;    // 0x0E  sectors before the first FAT
    uint8_t  fat_count;           // 0x10  number of FAT copies (usually 2)
    uint16_t root_entry_count;    // 0x11  max root-directory entries
    uint16_t total_sectors_16;    // 0x13  total sectors (0 if > 65535)
    uint8_t  media_type;          // 0x15  0xF8 = fixed disk
    uint16_t fat_size_16;         // 0x16  sectors per FAT copy
    uint16_t sectors_per_track;   // 0x18  CHS (ignored with LBA)
    uint16_t head_count;          // 0x1A  CHS (ignored with LBA)
    uint32_t hidden_sectors;      // 0x1C  sectors before this partition
    uint32_t total_sectors_32;    // 0x20  total sectors if > 65535

    // FAT16 extended BPB (offset 0x24)
    uint8_t  drive_number;        // 0x24  0x80 = first HDD
    uint8_t  reserved1;           // 0x25
    uint8_t  boot_signature;      // 0x26  0x29 → serial/label/type valid
    uint32_t volume_id;           // 0x27  random serial number
    uint8_t  volume_label[11];    // 0x2B  "YOTOS      " (space-padded)
    uint8_t  fs_type[8];          // 0x36  "FAT16   "
} fat16_bpb_t;


// 32byte directory entry
typedef struct __attribute__((packed)) {
    uint8_t  name[8];          // 0x00  base name (space-padded, NOT null-terminated)
    uint8_t  ext[3];           // 0x08  extension (space-padded)
    uint8_t  attributes;       // 0x0B  FAT16_ATTR_* flags
    uint8_t  reserved[10];     // 0x0C  creation timestamps + cluster-high word (FAT32)
    uint16_t write_time;       // 0x16  last-write time
    uint16_t write_date;       // 0x18  last-write date
    uint16_t first_cluster;    // 0x1A  first data cluster (0 = empty)
    uint32_t file_size;        // 0x1C  size in bytes
} fat16_direntry_t;




// Public API

// Read and validate the boot sector; compute FAT/root/data layout constants.
// Must be called after ata_init() and before fat16_vfs_root().
// Returns 0 on success, -1 on failure.
int fat16_init();

// Return the static VFS root node for the FAT16 volume.
// Pass the result to vfs_mount("/", fat16_vfs_root()).
// Do NOT call vfs_close() on the returned pointer.
vfs_node_t *fat16_vfs_root();

#endif

#include "ata.h"
#include "io.h"
#include "stdio.h"
#include <stdint.h>

#define ATA_DATA        0x1F0   // 16-bit data register
#define ATA_FEATURES    0x1F1   // write: features   read: error
#define ATA_SECTOR_CNT  0x1F2   // sector count
#define ATA_LBA_LO      0x1F3   // LBA bits  7:0
#define ATA_LBA_MID     0x1F4   // LBA bits 15:8
#define ATA_LBA_HI      0x1F5   // LBA bits 23:16
#define ATA_DRIVE_HEAD  0x1F6   // drive/head + LBA bits 27:24
#define ATA_STATUS      0x1F7   // read: status
#define ATA_COMMAND     0x1F7   // write: command
#define ATA_ALT_STATUS  0x3F6   // alternate status (no side effects)

#define ATA_SR_BSY  0x80    // drive is busy
#define ATA_SR_DRQ  0x08    // data request ready
#define ATA_SR_ERR  0x01    // error occurred

#define ATA_CMD_READ_PIO  0x20  // read sectors (with retry)
#define ATA_CMD_WRITE_PIO 0x30  // write sectors (with retry)
#define ATA_CMD_IDENTIFY  0xEC  // identify drive

// Which drive on the primary bus: 0 = master, 1 = slave
// The makefile attaches disk.img at index=1 (primary slave).
static uint8_t current_drive = 1;



// Burn 400 ns by reading the alt-status register four times (standard ATA delay).
static inline void ata_delay400ns() {
    inb(ATA_ALT_STATUS);
    inb(ATA_ALT_STATUS);
    inb(ATA_ALT_STATUS);
    inb(ATA_ALT_STATUS);
}

// Spin until BSY clears.
static void ata_wait_bsy() {
    while (inb(ATA_STATUS) & ATA_SR_BSY);
}

// Spin until DRQ sets or ERR sets.  Returns 0 on success, -1 on error/timeout.
static int ata_wait_drq() {
    for (int i = 0; i < 1000000; i++) {
        uint8_t s = inb(ATA_STATUS);
        if (s & ATA_SR_ERR) return -1;
        if (s & ATA_SR_DRQ) return 0;
    }
    return -1;   // timeout
}




// ata_read_sector - read one 512-byte sector using 28-bit LBA PIO mode.

void ata_read_sector(uint32_t lba, uint8_t *buf) {
    ata_wait_bsy();

    // Select drive + upper 4 LBA bits.
    // 0xE0 = master LBA mode,  0xF0 = slave LBA mode
    outb(ATA_DRIVE_HEAD,
         (current_drive == 0 ? 0xE0 : 0xF0) | ((lba >> 24) & 0x0F));

    outb(ATA_FEATURES,   0x00);          // no special features
    outb(ATA_SECTOR_CNT, 1);             // read 1 sector
    outb(ATA_LBA_LO,  (uint8_t)(lba));
    outb(ATA_LBA_MID, (uint8_t)(lba >> 8));
    outb(ATA_LBA_HI,  (uint8_t)(lba >> 16));
    outb(ATA_COMMAND, ATA_CMD_READ_PIO);

    ata_delay400ns();

    if (ata_wait_drq() < 0) {
        // Error or timeout: zero out the buffer so callers get clean data
        for (int i = 0; i < 512; i++) buf[i] = 0;
        return;
    }

    // Read 256 × 16-bit words = 512 bytes
    uint16_t *w = (uint16_t *)buf;
    for (int i = 0; i < 256; i++)
        w[i] = inw(ATA_DATA);
}


void ata_write_sector(uint32_t lba, uint8_t *buf) {
    ata_wait_bsy();

    outb(ATA_DRIVE_HEAD,
         (current_drive == 0 ? 0xE0 : 0xF0) | ((lba >> 24) & 0x0F));

    outb(ATA_FEATURES,   0x00);
    outb(ATA_SECTOR_CNT, 1);
    outb(ATA_LBA_LO,  (uint8_t)(lba));
    outb(ATA_LBA_MID, (uint8_t)(lba >> 8));
    outb(ATA_LBA_HI,  (uint8_t)(lba >> 16));
    outb(ATA_COMMAND, ATA_CMD_WRITE_PIO);

    ata_delay400ns();

    if (ata_wait_drq() < 0) return;

    // Write 256 × 16-bit words = 512 bytes
    uint16_t *w = (uint16_t *)buf;
    for (int i = 0; i < 256; i++)
        outw(ATA_DATA, w[i]);

    ata_wait_bsy();
}


/*
 * ata_init - detect which drive holds the FAT16 disk image and set
 * current_drive accordingly.
 *
 * Strategy: read sector 0 from the slave (index=1); if the
 * standard boot-sector signature 0x55AA is absent, fall back to master.
 * Prints a one-line result regardless.
 */
void ata_init() {
    uint8_t buf[512];

    // Try slave first (matches makefile's index=1)
    current_drive = 1;
    ata_read_sector(0, buf);
    if (buf[510] == 0x55 && buf[511] == 0xAA) {
        printf("[ATA] Disk found on primary slave\n");
        return;
    }

    // Fall back to master
    current_drive = 0;
    ata_read_sector(0, buf);
    if (buf[510] == 0x55 && buf[511] == 0xAA) {
        printf("[ATA] Disk found on primary master\n");
        return;
    }

    printf("[ATA] No disk found (checked slave + master)\n");
}

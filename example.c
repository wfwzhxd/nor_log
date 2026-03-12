/*
 * Example usage of NOR log library
 *
 * This demonstrates how to use the nor_log library with custom flash operations.
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "nor_log.h"

/* Simple memory-based flash emulation */
static uint8_t flash_memory[4096];  /* 4KB flash memory */

/* Flash write function */
static void example_flash_write(uint32_t addr, const void *buf, uint32_t len)
{
    if (addr + len <= sizeof(flash_memory)) {
        memcpy(&flash_memory[addr], buf, len);
    }
}

/* Flash read function */
static void example_flash_read(uint32_t addr, void *buf, uint32_t len)
{
    if (addr + len <= sizeof(flash_memory)) {
        memcpy(buf, &flash_memory[addr], len);
    }
}

/* Example hash function - CRC16-CCITT implementation */
static uint16_t example_hash_func(const void *data, uint32_t len)
{
    uint16_t crc = HASH_INIT;
    const uint8_t *ptr = (const uint8_t *)data;
    
    while (len--) {
        crc ^= (*ptr++ << 8);
        for (int i = 0; i < 8; i++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

/* Extended log entry structure for this example */
typedef struct
{
    uint32_t log_id;    /* log_id must be first (inherits from base_log_entry_t) */
    uint16_t crc16;     /* crc16 must be second (inherits from base_log_entry_t) */
    uint32_t timestamp;
    char message[32];
} example_log_entry_t;

int main(void)
{
    /* Initialize flash memory to 0xFF (erased state) */
    memset(flash_memory, 0xFF, sizeof(flash_memory));
    
    /* Set up log context */
    nor_log_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.first_entry_addr = 0;  /* Start of flash memory */
    ctx.sizeof_log_entry = sizeof(example_log_entry_t);
    /* Calculate last address to ensure proper alignment */
    ctx.last_entry_addr = ctx.first_entry_addr + (16 * ctx.sizeof_log_entry) - ctx.sizeof_log_entry;
    ctx.flash_write = example_flash_write;
    ctx.flash_read = example_flash_read;
    ctx.hash_func = example_hash_func;
    
    /* Temporary buffer for initialization */
    example_log_entry_t tmp_entry;
    
    /* Initialize the log */
    nor_log_init(&ctx, (base_log_entry_t *)&tmp_entry);
    
    printf("Log initialized:\n");
    printf("  First entry ID: %u\n", ctx.first_entry_id);
    printf("  Next write address: 0x%08X\n", ctx.next_entry_addr);
    
    /* Create and append some log entries */
    for (int i = 0; i < 5; i++) {
        example_log_entry_t entry;
        memset(&entry, 0, sizeof(entry));
        entry.timestamp = i * 1000;
        snprintf(entry.message, sizeof(entry.message), "Test message %d", i);
        
        nor_log_append(&ctx, (base_log_entry_t *)&entry);
        
        printf("Appended entry %d with ID %u\n", i, entry.log_id);
    }
    
    /* Read back entries */
    printf("\nReading back entries:\n");
    for (uint32_t idx = 0; idx < 5; idx++) {
        example_log_entry_t read_entry;
        bool success = nor_log_read(&ctx, idx, (base_log_entry_t *)&read_entry);
        
        if (success) {
            printf("Entry %u: ID=%u, timestamp=%u, message='%s'\n",
                   idx, read_entry.log_id, read_entry.timestamp, read_entry.message);
        } else {
            printf("Entry %u: Failed to read\n", idx);
        }
    }
    
    return 0;
}

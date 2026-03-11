/*
 * NOR Flash circular log test module
 *
 * This module contains the self-test for the NOR log implementation.
 * It validates the circular log functionality by:
 * 1. Initializing flash memory to erased state (0xFF)
 * 2. Setting up log context with the entire flash memory range
 * 3. Continuously writing log entries with incremental data
 * 4. Verifying that addresses are correctly calculated
 */

#include <stdint.h>
#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include "nor_log.h"

/* Helper macro to get the number of elements in an array */
#define ARRAY_SIZE(ARRAY) (sizeof(ARRAY) / sizeof(ARRAY[0]))

/*
 * Flash memory emulation structure
 *
 * This union provides two views of the same memory:
 * 1. sectors[256] - Organized as 256 sectors, each with 4 log entries
 * 2. all_log_entry[1024] - Flat array of 1024 (256*4) log entries
 *
 * Each log entry consists of:
 *   - id: 32-bit log entry identifier
 *   - crc16: 16-bit CRC checksum
 *   - reserved: 16-bit padding for alignment
 *   - data[14]: 56 bytes of payload data
 * Total size: 64 bytes (4 + 2 + 2 + 56 = 64)
 */
union
{
    struct
    {
        struct
        {
            uint32_t id;
            uint16_t crc16;
            uint16_t reserved;
            uint32_t data[14];
        } log_entry[4];
    } sectors[256];
    struct
    {
        uint32_t id;
        uint16_t crc16;
        uint16_t reserved;
        uint32_t data[14];
    } all_log_entry[4 * 256];
} flash_ram;

/* Convenience alias for accessing sectors */
#define sectors (flash_ram.sectors)

/*
 * Flash write operation with sector erase emulation
 *
 * NOR flash requires erase before write. This macro emulates that behavior:
 * - If address is at a sector boundary (256-byte aligned), erase the sector
 * - Then perform the write operation
 *
 * The address manipulation handles the difference between emulated flash
 * addresses and actual RAM addresses.
 */
static void test_flash_write(uint32_t addr, const void *buf, uint32_t len)
{
    if ((addr - (0xFFFFFFFF & ((size_t)&sectors[0].log_entry[0]))) % 256 == 0)
    {
        memset(((void *)(addr + (0xFFFFFFFF00000000 & ((size_t)&sectors)))), 0, 256);
    }
    memcpy(((void *)(addr + (0xFFFFFFFF00000000 & ((size_t)&sectors)))), buf, len);
}

/* Flash read operation - simple memory copy with address translation */
static void test_flash_read(uint32_t addr, void *buf, uint32_t len)
{
    memcpy(buf, ((void *)(addr + (0xFFFFFFFF00000000 & ((size_t)&sectors)))), len);
}

/*
 * CRC16 checksum stub
 *
 * This is a placeholder implementation. In a real system, this should be
 * replaced with an actual CRC16 calculation function.
 */
#ifndef CRC16
#define CRC16(data, len) (0xcc16)
#endif

/*
 * CRC16 initial value for calculation
 *
 * Standard CRC16 algorithms often use 0xFFFF as initial value.
 * This can be customized for different CRC16 variants.
 */
#ifndef CRC16_INIT
#define CRC16_INIT 0xFFFF
#endif

/* Test context and state variables */
static nor_log_ctx_t my_ctx;           /* Log context for testing */
static bool do_init = true;            /* Flag to force initialization on first iteration */
static uint32_t saved_next_addr = UINT32_MAX;  /* Previous next_entry_addr for validation */

/*
 * Self-test function
 *
 * This test validates the NOR log implementation by:
 * 1. Initializing flash memory to erased state (0xFF)
 * 2. Setting up log context with the entire flash memory range
 * 3. Continuously writing log entries with incremental data
 * 4. Verifying that addresses are correctly calculated
 *
 * The test runs indefinitely (UINT32_MAX iterations) and can be
 * interrupted with Ctrl+C.
 */
int main(void)
{
    /* Initialize flash memory to erased state (all 0xFF) */
    memset(&flash_ram, 0xFF, sizeof(flash_ram));
    uint32_t entry[16];  /* Buffer for log entry (16 * 4 = 64 bytes) */
    
    /* Infinite test loop */
    for (size_t i = 0; i < UINT32_MAX; i++)
    {
        /* Initialize context on first iteration */
        if (do_init)
        {
            memset(&my_ctx, 0, sizeof(my_ctx));
            my_ctx.first_entry_addr = 0xFFFFFFFF & ((size_t)&sectors[0].log_entry[0]);
            my_ctx.last_entry_addr = 0xFFFFFFFF & ((size_t)&sectors[ARRAY_SIZE(sectors) - 1].log_entry[3]);
            my_ctx.sizeof_log_entry = sizeof(sectors[0].log_entry[0]);
            my_ctx.flash_write = test_flash_write;
            my_ctx.flash_read = test_flash_read;
            nor_log_init(&my_ctx, (base_log_entry_t *)entry);
            do_init = false;
        }
        
        /* Verify next_entry_addr matches expected location in flat array */
        assert(my_ctx.next_entry_addr == (0xFFFFFFFF & ((size_t)&flash_ram.all_log_entry[i % ARRAY_SIZE(flash_ram.all_log_entry)])));
        
        /* After first iteration, verify consistency with saved address */
        if (UINT32_MAX != saved_next_addr)
        {
            assert(saved_next_addr == my_ctx.next_entry_addr);
        }

        /* Prepare log entry with test data */
        memset(entry, 0, sizeof(entry));
        entry[2] = 100 + i;  /* Store iteration count in data field */
        
        /* Append the log entry */
        uint32_t write_addr = my_ctx.next_entry_addr;
        nor_log_append(&my_ctx, (base_log_entry_t *)entry);
        
        /* Read back and verify the entry */
        uint32_t idx = (write_addr - my_ctx.first_entry_addr) / my_ctx.sizeof_log_entry;
        uint32_t read_back[16];
        bool success = nor_log_read(&my_ctx, idx, (base_log_entry_t *)read_back);
        assert(success);
        assert(memcmp(entry, read_back, my_ctx.sizeof_log_entry) == 0);
        
        /* Save next_entry_addr for next iteration's validation */
        saved_next_addr = my_ctx.next_entry_addr;
    }
    
    return 0;
}

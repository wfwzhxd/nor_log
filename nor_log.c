
/*
 * NOR Flash circular log implementation
 *
 * This module implements a circular log on NOR flash memory.
 * It provides functions to initialize, append, and manage log entries
 * in a wear-leveling friendly manner.
 */

#include <stdint.h>
#include <assert.h>
#include <stdbool.h>
#include <string.h>

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
#define flash_write(addr, buf, len)                                                       \
    do                                                                                    \
    {                                                                                     \
        if ((addr - (0xFFFFFFFF & ((size_t)&sectors[0].log_entry[0]))) % 256 == 0)        \
        {                                                                                 \
            memset(((void *)(addr + (0xFFFFFFFF00000000 & ((size_t)&sectors)))), 0, 256); \
        }                                                                                 \
        memcpy(((void *)(addr + (0xFFFFFFFF00000000 & ((size_t)&sectors)))), buf, len);   \
    } while (0)

/* Flash read operation - simple memory copy with address translation */
#define flash_read(addr, buf, len) memcpy(buf, ((void *)(addr + (0xFFFFFFFF00000000 & ((size_t)&sectors)))), len)

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
 * Log entry structure
 *
 * Total size: 64 bytes (matching flash_ram.log_entry)
 * Layout:
 *   log_id:  4 bytes (32-bit identifier)
 *   crc16:   2 bytes (CRC16 checksum)
 *   reserved: 2 bytes (padding for alignment)
 *   data:    56 bytes (14 x uint32_t payload)
 */
typedef struct
{
    uint32_t log_id;          /* Unique identifier for this log entry */
    uint16_t crc16;           /* CRC16 checksum for data integrity */
    uint16_t reserved;        /* Padding for 64-bit alignment */
    uint32_t data[14];        /* Payload data (56 bytes) */
} base_log_entry_t;

/* NOR log context structure */
typedef struct
{
    uint32_t first_entry_addr;  /* Address of first log entry in flash */
    uint32_t last_entry_addr;   /* Address of last log entry in flash */
    uint32_t sizeof_log_entry;  /* Size of each log entry in bytes */
    uint32_t first_entry_id;    /* ID of the first entry (0 or 1) */
    uint32_t next_entry_addr;   /* Address where next entry will be written */
} nor_log_ctx_t;

/* Read the log entry ID from a given flash address */
static inline uint32_t read_entry_id(uint32_t addr)
{
    uint32_t id;
    flash_read(addr, &id, sizeof(id));
    return id;
}

/*
 * Address/ID conversion macros
 *
 * These macros convert between flash addresses and log entry IDs.
 * The conversion assumes entries are contiguous in flash memory.
 */

/* Convert flash address to log entry ID */
#define entry_addr2id(ctx, address) (ctx->first_entry_id + (address - ctx->first_entry_addr) / ctx->sizeof_log_entry)

/* Convert log entry ID to flash address */
#define entry_id2addr(ctx, id) (ctx->first_entry_addr + ((id) - ctx->first_entry_id) * ctx->sizeof_log_entry)

/* Check if an address contains a valid log entry ID */
#define is_address_has_valid_entry_id(ctx, address) (entry_id2addr(ctx, read_entry_id(address)) == address)

/*
 * Initialize the log context and determine the next write address
 *
 * This function analyzes the current state of the flash memory and sets up
 * the context for subsequent append operations. It handles three cases:
 * 1. Empty log (all entries erased)
 * 2. Full log (all entries written)
 * 3. Partially filled log (some entries written)
 *
 * For partially filled logs, it uses binary search to find the last valid
 * entry, which determines where the next entry should be written.
 */
void nor_log_init_next_entry_addr(nor_log_ctx_t *ctx)
{
    /* Ensure the address range is properly aligned to entry size */
    assert((ctx->last_entry_addr - ctx->first_entry_addr) % ctx->sizeof_log_entry == 0);

    /* Read the ID from the first entry to determine log state */
    ctx->first_entry_id = read_entry_id(ctx->first_entry_addr);

    /* If first entry has ID 0 or 1, log follows our convention */
    if ((0 == ctx->first_entry_id) || (1 == ctx->first_entry_id))
    {
        /* Check if the last entry is valid (log is full) */
        if (is_address_has_valid_entry_id(ctx, ctx->last_entry_addr))
        {
            /* Log is full - wrap around to beginning */
            ctx->first_entry_id = ctx->first_entry_id ? 0 : 1;
            ctx->next_entry_addr = ctx->first_entry_addr;
        }
        else
        {
            /* Log is partially filled - find last valid entry using binary search */
            uint32_t left = entry_addr2id(ctx, ctx->first_entry_addr);
            uint32_t right = entry_addr2id(ctx, ctx->last_entry_addr);
            
            while (right >= left)
            {
                uint32_t mid = left + (right - left) / 2;
                if (is_address_has_valid_entry_id(ctx, entry_id2addr(ctx, mid)))
                {
                    /* mid is valid, search right half */
                    left = mid + 1;
                }
                else
                {
                    /* mid is invalid, search left half */
                    right = mid - 1;
                }
            }
            
            /* left points to first invalid entry, which is where we write next */
            ctx->next_entry_addr = entry_id2addr(ctx, left);
        }
    }
    else
    {
        /* First entry has invalid ID - treat as empty log */
        ctx->first_entry_id = ctx->first_entry_id ? 0 : 1;
        ctx->next_entry_addr = ctx->first_entry_addr;
    }
}

/*
 * Append a new log entry to the circular buffer
 *
 * This function writes a log entry to flash and updates the context.
 * It handles CRC calculation and circular buffer wrap-around.
 *
 * Steps:
 * 1. Assign the next available log ID
 * 2. Calculate CRC16 checksum (with zero placeholder, then compute)
 * 3. Write entry to flash
 * 4. Update next write address, handling wrap-around if necessary
 */
void nor_log_append(nor_log_ctx_t *ctx, base_log_entry_t *log_entry)
{
    /* Determine the log ID for this entry based on write address */
    uint32_t id = entry_addr2id(ctx, ctx->next_entry_addr);
    log_entry->log_id = id;
    
    /* Calculate CRC16 checksum */
    log_entry->crc16 = 0;      /* Temporary zero for CRC calculation */
    log_entry->reserved = 0;   /* Ensure reserved field is zero for consistent CRC */
    uint16_t computed_crc = CRC16((const void*)log_entry, ctx->sizeof_log_entry);
    log_entry->crc16 = computed_crc;
    
    /* Write the entry to flash memory */
    flash_write(ctx->next_entry_addr, log_entry, ctx->sizeof_log_entry);
    
    /* Advance to next write address */
    ctx->next_entry_addr += ctx->sizeof_log_entry;
    
    /* Handle circular buffer wrap-around */
    if (ctx->next_entry_addr > ctx->last_entry_addr)
    {
        ctx->first_entry_id = ctx->first_entry_id ? 0 : 1;
        ctx->next_entry_addr = ctx->first_entry_addr;
    }
}

/* Test context and state variables */
nor_log_ctx_t my_ctx;           /* Log context for testing */
bool do_init = true;            /* Flag to force initialization on first iteration */
uint32_t saved_next_addr = UINT32_MAX;  /* Previous next_entry_addr for validation */

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
            nor_log_init_next_entry_addr(&my_ctx);
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
        nor_log_append(&my_ctx, (base_log_entry_t *)entry);

        /* Save next_entry_addr for next iteration's validation */
        saved_next_addr = my_ctx.next_entry_addr;
    }
}


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
#include "nor_log.h"

/* Helper macro to get the number of elements in an array */
#define ARRAY_SIZE(ARRAY) (sizeof(ARRAY) / sizeof(ARRAY[0]))

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



/* Static function declarations */
static bool nor_log_checkcrc(nor_log_ctx_t *ctx, base_log_entry_t *log_entry);

/*
 * Check if an entry at given address is valid (CRC only)
 *
 * This function checks only CRC validity, without verifying log id consistency.
 * Used during initialization when first_entry_id is not yet known.
 *
 * Parameters:
 *   ctx - Log context
 *   addr - Flash address to check
 *   tmp_log_entry - Buffer for reading the entry (at least ctx->sizeof_log_entry bytes)
 * Returns:
 *   true if entry has valid CRC, false otherwise
 */
static inline bool is_entry_crc_valid_at_address(nor_log_ctx_t *ctx, uint32_t addr, base_log_entry_t *tmp_log_entry)
{
    /* Read entire entry into caller-provided buffer */
    ctx->flash_read(addr, tmp_log_entry, ctx->sizeof_log_entry);
    
    /* Check CRC validity only */
    return nor_log_checkcrc(ctx, tmp_log_entry);
}

/*
 * Check if an entry at given address is valid
 *
 * An entry is considered valid if:
 * 1. The log_id at the address maps back to the same address (log id consistency)
 * 2. The CRC16 checksum is valid for the entire entry
 *
 * Parameters:
 *   ctx - Log context
 *   addr - Flash address to check
 *   tmp_log_entry - Buffer for reading the entry (at least ctx->sizeof_log_entry bytes)
 * Returns:
 *   true if entry is valid, false otherwise
 */
static inline bool is_entry_valid_at_address(nor_log_ctx_t *ctx, uint32_t addr, base_log_entry_t *tmp_log_entry)
{
    /* Read entire entry into caller-provided buffer */
    ctx->flash_read(addr, tmp_log_entry, ctx->sizeof_log_entry);
    
    /* Check log id consistency */
    if (entry_id2addr(ctx, tmp_log_entry->log_id) != addr) {
        return false;
    }
    
    /* Check CRC validity */
    return nor_log_checkcrc(ctx, tmp_log_entry);
}

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
 *
 * Parameters:
 *   ctx - Log context to initialize (user must set flash_write and flash_read)
 *   tmp_log_entry - Buffer for temporary storage of log entries during scanning.
 *                   Must be at least ctx->sizeof_log_entry bytes in size.
 *                   The buffer contents may be modified by this function.
 */
void nor_log_init(nor_log_ctx_t *ctx, base_log_entry_t *tmp_log_entry)
{
    /* Ensure the address range is properly aligned to entry size */
    assert((ctx->last_entry_addr - ctx->first_entry_addr) % ctx->sizeof_log_entry == 0);
    /* Ensure flash operation functions are set */
    assert(ctx->flash_write != NULL);
    assert(ctx->flash_read != NULL);

    /* Check if the first entry has valid CRC (log id consistency checked later) */
    if (is_entry_crc_valid_at_address(ctx, ctx->first_entry_addr, tmp_log_entry))
    {
        /* First entry is valid, get its ID */
        ctx->first_entry_id = tmp_log_entry->log_id;
        
        /* If first entry has ID 0 or 1, log follows our convention */
        if ((0 == ctx->first_entry_id) || (1 == ctx->first_entry_id))
        {
            /* Check if the last entry is valid (log is full) */
            if (is_entry_valid_at_address(ctx, ctx->last_entry_addr, tmp_log_entry))
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
                    if (is_entry_valid_at_address(ctx, entry_id2addr(ctx, mid), tmp_log_entry))
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
            /* First entry has valid CRC but ID is not 0 or 1 - treat as empty log */
            ctx->first_entry_id = 0;
            ctx->next_entry_addr = ctx->first_entry_addr;
        }
    }
    else
    {
        /* First entry is invalid (CRC error) - treat as empty log */
        ctx->first_entry_id = 0;
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
 *
 * Parameters:
 *   ctx - Initialized log context
 *   log_entry - Entry to append (log_id will be set by this function)
 */
void nor_log_append(nor_log_ctx_t *ctx, base_log_entry_t *log_entry)
{
    /* Determine the log ID for this entry based on write address */
    uint32_t id = entry_addr2id(ctx, ctx->next_entry_addr);
    log_entry->log_id = id;
    
    /* Calculate CRC16 checksum for the entire log entry */
    log_entry->crc16 = CRC16_INIT;  /* Set initial value for CRC calculation */
    uint16_t computed_crc = CRC16((const void*)log_entry, ctx->sizeof_log_entry);
    log_entry->crc16 = computed_crc;
    
    /* Write the entry to flash memory */
    ctx->flash_write(ctx->next_entry_addr, log_entry, ctx->sizeof_log_entry);
    
    /* Advance to next write address */
    ctx->next_entry_addr += ctx->sizeof_log_entry;
    
    /* Handle circular buffer wrap-around */
    if (ctx->next_entry_addr > ctx->last_entry_addr)
    {
        ctx->first_entry_id = ctx->first_entry_id ? 0 : 1;
        ctx->next_entry_addr = ctx->first_entry_addr;
    }
}

/*
 * Read a log entry by index
 *
 * This function reads a log entry at the specified index into the provided buffer.
 * The index is zero-based, corresponding to the first entry in the log.
 * 
 * Parameters:
 *   ctx - Log context
 *   log_entry_idx - Zero-based index of the entry to read
 *   log_entry - Buffer to store the read entry (at least ctx->sizeof_log_entry bytes)
 * Returns:
 *   true if the index is valid and the entry has valid CRC and log id consistency,
 *   false otherwise (invalid index or corrupted entry)
 */
bool nor_log_read(nor_log_ctx_t *ctx, uint32_t log_entry_idx, base_log_entry_t *log_entry)
{
    /* Calculate maximum valid index */
    uint32_t max_idx = (ctx->last_entry_addr - ctx->first_entry_addr) / ctx->sizeof_log_entry;
    
    /* Check if index is within valid range */
    if (log_entry_idx > max_idx)
    {
        return false;
    }
    
    /* Calculate address from index */
    uint32_t addr = ctx->first_entry_addr + log_entry_idx * ctx->sizeof_log_entry;
    
    /* Read entry into buffer */
    ctx->flash_read(addr, log_entry, ctx->sizeof_log_entry);
    
    /* Check CRC validity first */
    if (!nor_log_checkcrc(ctx, log_entry)) {
        return false;
    }
    
    /* Check log id consistency - allow for first_entry_id being 0 or 1 */
    uint32_t offset = (addr - ctx->first_entry_addr) / ctx->sizeof_log_entry;
    uint32_t fid = log_entry->log_id - offset;
    return (fid == 0 || fid == 1);
}

/*
 * Check CRC integrity of a log entry
 *
 * This function verifies the CRC checksum of a log entry.
 * Steps:
 * 1. Save the original CRC value
 * 2. Set crc16 field to CRC16_INIT (same as during calculation)
 * 3. Compute CRC over the entire entry
 * 4. Restore original CRC value (non-destructive check)
 * 5. Return true if computed CRC matches original CRC
 */
static bool nor_log_checkcrc(nor_log_ctx_t *ctx, base_log_entry_t *log_entry)
{
    uint16_t original_crc = log_entry->crc16;
    log_entry->crc16 = CRC16_INIT;
    uint16_t computed_crc = CRC16((const void*)log_entry, ctx->sizeof_log_entry);
    log_entry->crc16 = original_crc;  /* Restore original value */
    (void)ctx;  /* ctx is used via ctx->sizeof_log_entry in CRC16 macro */
    return computed_crc == original_crc;
}

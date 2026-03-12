
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
static bool nor_log_checkhash(nor_log_ctx_t *ctx, base_log_entry_t *log_entry);

/*
 * Check if an entry at given address is valid (hash only)
 *
 * This function checks only hash validity, without verifying log id consistency.
 * Used during initialization when first_entry_id is not yet known.
 *
 * Parameters:
 *   ctx - Log context
 *   addr - Flash address to check
 *   tmp_log_entry - Buffer for reading the entry (at least ctx->sizeof_log_entry bytes)
 * Returns:
 *   true if entry has valid hash, false otherwise
 */
static inline bool is_entry_hash_valid_at_address(nor_log_ctx_t *ctx, uint32_t addr, base_log_entry_t *tmp_log_entry)
{
    /* Read entire entry into caller-provided buffer */
    ctx->flash_read(addr, tmp_log_entry, ctx->sizeof_log_entry);
    
    /* Check hash validity only */
    return nor_log_checkhash(ctx, tmp_log_entry);
}

/*
 * Check if an entry at given address is valid
 *
 * An entry is considered valid if:
 * 1. The log_id at the address maps back to the same address (log id consistency)
 * 2. The hash/checksum is valid for the entire entry
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
    
    /* Check hash validity */
    return nor_log_checkhash(ctx, tmp_log_entry);
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
    /* Ensure flash operation functions and hash function are set */
    assert(ctx->flash_write != NULL);
    assert(ctx->flash_read != NULL);
    assert(ctx->hash_func != NULL);

    /* Check if the first entry has valid hash (log id consistency checked later) */
    if (is_entry_hash_valid_at_address(ctx, ctx->first_entry_addr, tmp_log_entry))
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
    
    /* Calculate hash/checksum for the entire log entry */
    log_entry->hash = HASH_INIT;  /* Set initial value for hash calculation */
    uint16_t computed_hash = ctx->hash_func((const void*)log_entry, ctx->sizeof_log_entry);
    log_entry->hash = computed_hash;
    
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
    
    /* Check hash validity first */
    if (!nor_log_checkhash(ctx, log_entry)) {
        return false;
    }
    
    /* Check log id consistency - allow for first_entry_id being 0 or 1 */
    uint32_t offset = (addr - ctx->first_entry_addr) / ctx->sizeof_log_entry;
    uint32_t fid = log_entry->log_id - offset;
    return (fid == 0 || fid == 1);
}

/*
 * Check hash integrity of a log entry
 *
 * This function verifies the hash/checksum of a log entry.
 * Steps:
 * 1. Save the original hash value
 * 2. Set hash field to HASH_INIT (same as during calculation)
 * 3. Compute hash over the entire entry using ctx->hash_func
 * 4. Restore original hash value (non-destructive check)
 * 5. Return true if computed hash matches original hash
 */
static bool nor_log_checkhash(nor_log_ctx_t *ctx, base_log_entry_t *log_entry)
{
    uint16_t original_hash = log_entry->hash;
    log_entry->hash = HASH_INIT;
    uint16_t computed_hash = ctx->hash_func((const void*)log_entry, ctx->sizeof_log_entry);
    log_entry->hash = original_hash;  /* Restore original value */
    return computed_hash == original_hash;
}

#ifndef NOR_LOG_H
#define NOR_LOG_H

#include <stdint.h>
#include <stdbool.h>

/* Basic log entry structure - contains common fields */
typedef struct
{
    uint32_t log_id;    /* Unique identifier for this log entry */
    uint16_t crc16;     /* CRC16 checksum for data integrity */
} base_log_entry_t;

/* Flash operation function pointers */
typedef void (*flash_write_func_t)(uint32_t addr, const void *buf, uint32_t len);
typedef void (*flash_read_func_t)(uint32_t addr, void *buf, uint32_t len);

/* NOR log context structure */
typedef struct
{
    /* User-provided fields (must be set before calling nor_log_init) */
    uint32_t first_entry_addr;  /* Address of first log entry in flash */
    uint32_t last_entry_addr;   /* Address of last log entry in flash */
    uint32_t sizeof_log_entry;  /* Size of each log entry in bytes */
    flash_write_func_t flash_write;  /* Flash write function pointer */
    flash_read_func_t flash_read;    /* Flash read function pointer */
    
    /* Fields calculated by nor_log_init (do not set manually) */
    uint32_t first_entry_id;    /* ID of the first entry (0 or 1) */
    uint32_t next_entry_addr;   /* Address where next entry will be written */
} nor_log_ctx_t;

/* Public API functions */

/*
 * Initialize NOR log context and determine next write address.
 *
 * This function analyzes the flash memory state and sets up the context
 * for subsequent append operations. It determines whether the log is
 * empty, full, or partially filled, and calculates first_entry_id and
 * next_entry_addr accordingly.
 *
 * Parameters:
 *   ctx - Log context (user must set first_entry_addr, last_entry_addr,
 *         sizeof_log_entry, flash_write, flash_read before calling)
 *   tmp_log_entry - Buffer for temporary storage during scanning
 *                   (must be at least ctx->sizeof_log_entry bytes)
 */
void nor_log_init(nor_log_ctx_t *ctx, base_log_entry_t *tmp_log_entry);

/*
 * Append a new log entry to the circular buffer.
 *
 * This function writes a log entry to flash with proper CRC calculation
 * and updates the context. Handles circular buffer wrap-around automatically.
 *
 * Parameters:
 *   ctx - Initialized log context
 *   log_entry - Entry to append (log_id will be set by this function)
 */
void nor_log_append(nor_log_ctx_t *ctx, base_log_entry_t *log_entry);

/*
 * Read a log entry by index.
 *
 * This function reads a log entry at the specified zero-based index
 * into the provided buffer. The entry is validated for CRC integrity
 * and log id consistency.
 *
 * Parameters:
 *   ctx - Initialized log context
 *   log_entry_idx - Zero-based index of the entry to read
 *   log_entry - Buffer to store the read entry
 *                (must be at least ctx->sizeof_log_entry bytes)
 * Returns:
 *   true if index is valid and entry has valid CRC and log id consistency,
 *   false otherwise (invalid index or corrupted entry)
 */
bool nor_log_read(nor_log_ctx_t *ctx, uint32_t log_entry_idx, base_log_entry_t *log_entry);

#endif /* NOR_LOG_H */

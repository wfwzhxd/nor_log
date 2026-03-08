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

/* NOR log context structure */
typedef struct
{
    uint32_t first_entry_addr;  /* Address of first log entry in flash */
    uint32_t last_entry_addr;   /* Address of last log entry in flash */
    uint32_t sizeof_log_entry;  /* Size of each log entry in bytes */
    uint32_t first_entry_id;    /* ID of the first entry (0 or 1) */
    uint32_t next_entry_addr;   /* Address where next entry will be written */
} nor_log_ctx_t;

/* Public API functions */
void nor_log_init_next_entry_addr(nor_log_ctx_t *ctx, base_log_entry_t *tmp_log_entry);
void nor_log_append(nor_log_ctx_t *ctx, base_log_entry_t *log_entry);
bool nor_log_read(nor_log_ctx_t *ctx, base_log_entry_t *log_entry, uint32_t log_entry_idx);

#endif /* NOR_LOG_H */
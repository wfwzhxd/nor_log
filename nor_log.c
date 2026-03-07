
#include <stdint.h>
#include <assert.h>
#include <stdbool.h>
#include <string.h>

#define ARRAY_SIZE(ARRAY) (sizeof(ARRAY) / sizeof(ARRAY[0]))

union
{
    struct
    {
        struct
        {
            uint32_t id;
            uint32_t data[15];
        } log_entry[4];
    } sectors[256];
    struct
    {
        uint32_t id;
        uint32_t data[15];
    } all_log_entry[4 * 256];
} flash_ram;

#define sectors (flash_ram.sectors)

#define flash_write(addr, buf, len)                                                       \
    do                                                                                    \
    {                                                                                     \
        if ((addr - (0xFFFFFFFF & ((size_t)&sectors[0].log_entry[0]))) % 256 == 0)        \
        {                                                                                 \
            memset(((void *)(addr + (0xFFFFFFFF00000000 & ((size_t)&sectors)))), 0, 256); \
        }                                                                                 \
        memcpy(((void *)(addr + (0xFFFFFFFF00000000 & ((size_t)&sectors)))), buf, len);   \
    } while (0)

#define flash_read(addr, buf, len) memcpy(buf, ((void *)(addr + (0xFFFFFFFF00000000 & ((size_t)&sectors)))), len)

typedef struct
{
    uint32_t first_entry_addr;
    uint32_t last_entry_addr;
    uint32_t sizeof_log_entry;
    uint32_t first_entry_id;
    uint32_t next_entry_addr;
} nor_log_ctx_t;

static inline uint32_t read_entry_id(uint32_t addr)
{
    uint32_t id;
    flash_read(addr, &id, sizeof(id));
    return id;
}

#define entry_addr2id(ctx, address) (ctx->first_entry_id + (address - ctx->first_entry_addr) / ctx->sizeof_log_entry)
#define entry_id2addr(ctx, id) (ctx->first_entry_addr + ((id) - ctx->first_entry_id) * ctx->sizeof_log_entry)
#define is_address_has_valid_entry_id(ctx, address) (entry_id2addr(ctx, read_entry_id(address)) == address)

void nor_log_init_next_entry_addr(nor_log_ctx_t *ctx)
{
    assert((ctx->last_entry_addr - ctx->first_entry_addr) % ctx->sizeof_log_entry == 0);

    ctx->first_entry_id = read_entry_id(ctx->first_entry_addr);

    if ((0 == ctx->first_entry_id) || (1 == ctx->first_entry_id))
    {
        if (is_address_has_valid_entry_id(ctx, ctx->last_entry_addr))
        {
            ctx->first_entry_id = ctx->first_entry_id ? 0 : 1;
            ctx->next_entry_addr = ctx->first_entry_addr;
        }
        else
        {
            uint32_t left = entry_addr2id(ctx, ctx->first_entry_addr);
            uint32_t right = entry_addr2id(ctx, ctx->last_entry_addr);
            while (right >= left)
            {
                uint32_t mid = left + (right - left) / 2;
                if (is_address_has_valid_entry_id(ctx, entry_id2addr(ctx, mid)))
                {
                    left = mid + 1;
                }
                else
                {
                    right = mid - 1;
                }
            }
            ctx->next_entry_addr = entry_id2addr(ctx, left);
        }
    }
    else
    {
        ctx->first_entry_id = ctx->first_entry_id ? 0 : 1;
        ctx->next_entry_addr = ctx->first_entry_addr;
    }
}

void nor_log_append(nor_log_ctx_t *ctx, void *log_entry)
{
    uint32_t id = entry_addr2id(ctx, ctx->next_entry_addr);
    memcpy(log_entry, &id, sizeof(uint32_t));
    flash_write(ctx->next_entry_addr, log_entry, ctx->sizeof_log_entry);
    ctx->next_entry_addr += ctx->sizeof_log_entry;
    if (ctx->next_entry_addr > ctx->last_entry_addr)
    {
        ctx->first_entry_id = ctx->first_entry_id ? 0 : 1;
        ctx->next_entry_addr = ctx->first_entry_addr;
    }
}

nor_log_ctx_t my_ctx;
bool do_init = true;
uint32_t saved_next_addr = UINT32_MAX;
int main(void)
{
    memset(&flash_ram, 0xFF, sizeof(flash_ram));
    uint32_t entry[16];
    for (size_t i = 0; i < UINT32_MAX; i++)
    {
        if (do_init)
        {
            memset(&my_ctx, 0, sizeof(my_ctx));
            my_ctx.first_entry_addr = 0xFFFFFFFF & ((size_t)&sectors[0].log_entry[0]);
            my_ctx.last_entry_addr = 0xFFFFFFFF & ((size_t)&sectors[ARRAY_SIZE(sectors) - 1].log_entry[3]);
            my_ctx.sizeof_log_entry = sizeof(sectors[0].log_entry[0]);
            nor_log_init_next_entry_addr(&my_ctx);
        }
        assert(my_ctx.next_entry_addr == (0xFFFFFFFF & ((size_t)&flash_ram.all_log_entry[i % ARRAY_SIZE(flash_ram.all_log_entry)])));
        if (UINT32_MAX != saved_next_addr)
        {
            assert(saved_next_addr == my_ctx.next_entry_addr);
        }
        
        entry[1] = 100 + i;
        nor_log_append(&my_ctx, entry);

        saved_next_addr = my_ctx.next_entry_addr;
    }
}

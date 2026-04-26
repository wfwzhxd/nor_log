# NOR Flash Circular Log

一个用于NOR Flash的循环日志实现，使用C99编写。

## 项目背景

这个模块最初是人工编写的，后来借助 **opencode + deepseek** 体验 code agent 来不断完善的。通过AI辅助开发，我们实现了：
- 代码重构和模块化
- 测试代码分离
- 构建系统自动化
- 文档完善

**设计哲学**: 本模块的核心设计理念是**自包含性** - 不需要依赖外部存储来维护日志状态。传统的循环缓冲区通常需要在RAM或非易失性存储中保存读写指针，而本模块通过巧妙的ID编码和CRC校验，将所有状态信息都嵌入到日志条目本身，实现了真正的"无状态"日志管理。

## 功能特性

- **无额外存储**: **核心特色** - 不需要额外的存储介质（如EEPROM、FRAM等）来保存循环数组的索引，所有状态信息都编码在日志条目本身
- **循环日志**: 在NOR Flash上实现循环缓冲区，自动处理回绕
- **自包含状态**: 通过日志条目的ID和hash校验来重建日志状态，系统重启后能自动恢复
- **灵活校验**: 支持多种hash/checksum算法（CRC16、Adler-32等），通过函数指针由用户提供
- **磨损均衡**: 友好的磨损均衡设计，延长Flash寿命
- **简单API**: 易于集成和使用，只需提供Flash操作函数
- **平台无关**: 通过函数指针支持不同的Flash硬件

## 快速开始

### 为什么选择这个模块？

如果你正在寻找一个**轻量级、自包含、无需外部存储**的Flash日志解决方案，这个模块是理想选择。与传统的循环缓冲区实现相比，本模块：

1. **无需额外硬件**: 不需要EEPROM、FRAM或电池备份的RAM
2. **系统重启安全**: 断电后能自动恢复日志状态
3. **资源高效**: 最小化RAM使用，状态信息编码在Flash中
4. **易于集成**: 只需提供基本的Flash读写函数和hash计算函数

**注意**: 本模块使用hash/checksum进行数据完整性校验，通过函数指针由用户提供具体的hash实现。这提供了最大的灵活性，允许你选择最适合的校验算法（CRC16、CRC32、Adler-32等）。

### 编译和测试

```bash
# 构建所有目标
make all

# 运行测试（5秒超时）
make test

# 运行示例
make run-example
```

### 最简单的使用示例

```c
#include <stdint.h>
#include <string.h>
#include "nor_log.h"

/* 定义你的Flash操作函数 */
static void my_flash_write(uint32_t addr, const void *buf, uint32_t len) {
    /* 实现你的Flash写操作 */
}

static void my_flash_read(uint32_t addr, void *buf, uint32_t len) {
    /* 实现你的Flash读操作 */
}

/* 定义hash计算函数 - 需要用户自己实现 */
/* 示例：CRC16-CCITT实现（也可以使用其他算法如CRC32、Adler-32等） */
static uint16_t my_hash_func(const void *data, uint32_t len) {
    uint16_t crc = 0xFFFF;  /* CRC-16-CCITT init value */
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

/* 定义日志条目结构 */
typedef struct {
    uint32_t log_id;    /* 必须放在第一位（继承自base_log_entry_t） */
    uint16_t hash;      /* 必须放在第二位（继承自base_log_entry_t） */
    uint32_t timestamp;
    char message[32];
} my_log_entry_t;

int main(void) {
    /* 初始化上下文 */
    nor_log_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.first_entry_addr = 0x00000000;  /* Flash起始地址 */
    ctx.last_entry_addr = 0x00000FFF;   /* Flash结束地址 */
    ctx.sizeof_log_entry = sizeof(my_log_entry_t);
    ctx.flash_write = my_flash_write;
    ctx.flash_read = my_flash_read;
    ctx.hash_func = my_hash_func;  /* 设置hash函数指针 */
    
    /* 临时缓冲区 */
    my_log_entry_t tmp_entry;
    
    /* 初始化日志 */
    nor_log_init(&ctx, (base_log_entry_t *)&tmp_entry);
    
    /* 创建并追加日志条目 */
    my_log_entry_t entry;
    memset(&entry, 0, sizeof(entry));
    entry.timestamp = 1234567890;
    strcpy(entry.message, "Hello, NOR Log!");
    
    nor_log_append(&ctx, (base_log_entry_t *)&entry);
    
    /* 读取日志条目 */
    my_log_entry_t read_entry;
    bool success = nor_log_read(&ctx, 0, (base_log_entry_t *)&read_entry);
    
    if (success) {
        /* 处理读取的数据 */
    }
    
    return 0;
}
```

## API 文档

### 设计特色：无额外存储

本模块的**核心创新**在于不需要额外的存储介质来保存循环缓冲区的索引。传统实现通常需要：

1. **在RAM中保存指针** - 系统重启后丢失状态
2. **在EEPROM/FRAM中保存指针** - 增加硬件成本和复杂度
3. **在Flash的固定位置保存元数据** - 增加磨损和管理复杂度

**我们的解决方案**：
- **自包含状态**: 每个日志条目都包含唯一的`log_id`
- **ID编码规则**: ID序列要么从0开始递增，要么从1开始递增
- **状态恢复**: 系统重启后，通过扫描Flash并验证CRC和ID一致性来重建日志状态
- **二进制搜索**: 使用高效的二分查找算法快速定位最后一个有效条目

这种设计特别适合资源受限的嵌入式系统，减少了对外部存储的依赖，提高了系统的可靠性。

**关于hash/checksum的说明**：
本模块使用hash/checksum进行数据完整性校验，通过函数指针由用户提供具体的实现。这样做的好处是：
1. **最大灵活性**: 你可以选择最适合的校验算法（CRC16、CRC32、Adler-32、自定义算法等）
2. **硬件优化**: 可以根据硬件平台优化计算（如使用硬件CRC加速器）
3. **代码复用**: 可以与系统中其他模块共享相同的hash实现
4. **算法自由**: 不限制于CRC系列算法，可以使用任何16位hash函数

hash函数通过`nor_log_ctx_t.hash_func`函数指针设置，初始值通过`nor_log_ctx_t.hash_init`字段指定（例如CRC-16通常为`0xFFFF`）。

**集成方法**：
```c
// 1. 定义你的hash函数
static uint16_t my_hash_func(const void *data, uint32_t len) {
    // 实现你的hash算法（CRC16、CRC32、Adler-32等）
    return computed_hash;
}

// 2. 在初始化上下文时设置
nor_log_ctx_t ctx;
ctx.hash_func = my_hash_func;  // 设置hash函数指针
```

### 数据结构

#### `base_log_entry_t`
基本日志条目结构，所有自定义日志条目必须以此开头：
```c
typedef struct {
    uint32_t log_id;    /* 日志条目ID - 用于状态恢复的关键字段 */
    uint16_t hash;      /* hash/checksum值 - 确保数据完整性，使用用户提供的hash函数计算 */
} base_log_entry_t;

**重要**: `hash`字段的值由用户通过`nor_log_ctx_t.hash_func`函数指针提供的hash函数计算。hash计算应覆盖整个日志条目（包括`log_id`和`hash`字段本身）。计算前库会将`hash`字段置为`ctx->hash_init`（例如CRC-16通常为`0xFFFF`），然后再调用`hash_func`进行计算。

#### `nor_log_ctx_t`
日志上下文结构：
```c
typedef struct {
    /* 用户必须设置的字段 */
    uint32_t first_entry_addr;   /* 第一个日志条目的Flash地址 */
    uint32_t last_entry_addr;    /* 最后一个日志条目的Flash地址 */
    uint32_t sizeof_log_entry;   /* 每个日志条目的大小（字节） */
    flash_write_func_t flash_write;  /* Flash写函数指针 */
    flash_read_func_t flash_read;    /* Flash读函数指针 */
    hash_func_t hash_func;           /* hash/checksum函数指针 */
    
    /* 由nor_log_init计算的字段（不要手动设置） */
    uint32_t first_entry_id;     /* 第一个条目的ID（0或1） */
    uint32_t next_entry_addr;    /* 下一个写入地址 */
} nor_log_ctx_t;
```

### 函数接口

#### `void nor_log_init(nor_log_ctx_t *ctx, base_log_entry_t *tmp_log_entry)`
初始化日志上下文，分析Flash状态并确定下一个写入地址。

#### `void nor_log_append(nor_log_ctx_t *ctx, base_log_entry_t *log_entry)`
追加新的日志条目到循环缓冲区。

#### `bool nor_log_read(nor_log_ctx_t *ctx, uint32_t log_entry_idx, base_log_entry_t *log_entry)`
按索引读取日志条目，返回`true`表示成功。

## 构建系统

### Makefile 目标

- `make all` - 构建所有二进制文件（默认）
- `make test` - 构建并运行测试（5秒超时）
- `make run-example` - 构建并运行示例
- `make clean` - 清理构建产物
- `make help` - 显示帮助信息

### 编译选项

使用标准C99编译，启用所有警告：
```bash
gcc -g -Wall -Wextra -Werror -std=c99 -pedantic
```



## 项目结构

```
nor_log/
├── nor_log.c          # 核心库实现 - 包含无额外存储的智能算法
├── nor_log.h          # 库头文件 - 定义简洁的API接口
├── nor_log_test.c     # 测试代码 - 验证自包含状态恢复
├── example.c          # 使用示例 - 展示如何集成
├── Makefile           # 构建系统 - 简化编译过程
├── compile_commands.json  # 编译数据库（IDE支持）
└── README.md          # 本文档
```

**核心文件说明**:
- `nor_log.c`: 实现了不需要额外存储的状态恢复算法，通过ID编码和hash校验在系统重启后重建日志状态
- `nor_log.h`: 定义了简洁的API，用户只需提供Flash操作函数和hash函数指针，无需关心状态管理细节
- `nor_log_test.c`: 使用简单的hash存根进行测试，验证核心算法正确性
- `example.c`: 展示如何集成hash函数和Flash操作函数

## 开发工具

- **代码分析**: 使用 `compile_commands.json` 支持 clangd/ccls
- **静态检查**: 编译时启用 `-Wall -Wextra -Werror`
- **格式化**: 遵循 Allman 风格，4空格缩进

## 许可证

本项目采用 MIT 许可证。详见 LICENSE 文件（如有）。

## 贡献

这个项目展示了人工编写与AI辅助开发的结合：
1. **核心算法由人工设计实现** - 特别是"无额外存储"的状态恢复算法
2. **代码重构、测试和文档由AI辅助完成** - 通过opencode+deepseek优化代码结构
3. **构建系统和工具链自动化** - 提高开发效率

**设计亮点**:
- **创新性**: 解决了嵌入式系统中循环缓冲区需要外部存储的痛点
- **实用性**: 在资源受限的环境中特别有价值
- **可靠性**: 通过hash校验和ID一致性检查确保数据安全
- **最大灵活性**: 通过函数指针支持任意16位hash/checksum算法

**关于hash函数的设计决策**:
使用函数指针而不是宏定义或硬编码算法是一个深思熟虑的设计选择：
1. **算法自由**: 支持CRC16、CRC32、Adler-32、自定义算法等任意16位hash函数
2. **运行时配置**: 可以在运行时切换或配置hash算法
3. **硬件优化**: 允许使用硬件加速的计算单元
4. **代码复用**: 可以与系统中其他模块共享相同的hash实现
5. **减小依赖**: 不强制绑定到特定的hash算法库

欢迎反馈和改进建议！特别欢迎关于如何进一步优化状态恢复算法或扩展功能的建议。
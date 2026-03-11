# NOR Flash Circular Log

一个用于NOR Flash的循环日志实现，使用C99编写。

## 项目背景

这个模块最初是人工编写的，后来借助 **opencode + deepseek** 体验 code agent 来不断完善的。通过AI辅助开发，我们实现了：
- 代码重构和模块化
- 测试代码分离
- 构建系统自动化
- 文档完善

## 功能特性

- **循环日志**: 在NOR Flash上实现循环缓冲区
- **CRC校验**: 内置CRC16校验确保数据完整性
- **磨损均衡**: 友好的磨损均衡设计
- **简单API**: 易于集成和使用
- **平台无关**: 通过函数指针支持不同的Flash硬件

## 快速开始

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

/* 定义日志条目结构 */
typedef struct {
    uint32_t log_id;    /* 必须放在第一位（继承自base_log_entry_t） */
    uint16_t crc16;     /* 必须放在第二位（继承自base_log_entry_t） */
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

### 数据结构

#### `base_log_entry_t`
基本日志条目结构，所有自定义日志条目必须以此开头：
```c
typedef struct {
    uint32_t log_id;    /* 日志条目ID */
    uint16_t crc16;     /* CRC16校验值 */
} base_log_entry_t;
```

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
├── nor_log.c          # 核心库实现
├── nor_log.h          # 库头文件
├── nor_log_test.c     # 测试代码
├── example.c          # 使用示例
├── Makefile           # 构建系统
├── compile_commands.json  # 编译数据库（IDE支持）
└── README.md          # 本文档
```

## 开发工具

- **代码分析**: 使用 `compile_commands.json` 支持 clangd/ccls
- **静态检查**: 编译时启用 `-Wall -Wextra -Werror`
- **格式化**: 遵循 Allman 风格，4空格缩进

## 许可证

本项目采用 MIT 许可证。详见 LICENSE 文件（如有）。

## 贡献

这个项目展示了人工编写与AI辅助开发的结合：
1. 核心算法由人工设计实现
2. 代码重构、测试和文档由AI辅助完成
3. 构建系统和工具链自动化

欢迎反馈和改进建议！
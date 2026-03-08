# Agent Guidelines for nor_log

This document provides guidelines for AI agents working on the `nor_log` project, a NOR flash circular log implementation in C.

## Project Overview

- **Language**: C99
- **Source**: Single file `nor_log.c` with embedded test suite
- **Binary**: `nor_log` (ignored in `.gitignore`)
- **Build System**: Simple GCC compilation (no Makefile)
- **Testing**: Self‑test via `main()` function

## Build Commands

### Compile the binary
```bash
gcc -g -Wall -Wextra -Werror -std=c99 -pedantic -o nor_log nor_log.c
```

- `-g` adds debug symbols (used in VS Code tasks)
- `-Wall -Wextra -Werror` enable all warnings and treat them as errors
- `-std=c99` ensures C99 compatibility
- `-pedantic` rejects GNU extensions

### VS Code Task
The `.vscode/tasks.json` provides a default build task that compiles the active file with `-g` and diagnostic colors. Use it when working inside VS Code.

### Clean
```bash
rm -f nor_log
```

## Linting and Formatting

### Static Analysis
No dedicated linter configuration exists. You may run:
```bash
cppcheck --enable=all --suppress=missingIncludeSystem nor_log.c
```

### Code Formatting
No `.clang-format` or `.editorconfig` is present. Follow the existing indentation style:
- **Indentation**: 4 spaces (no tabs)
- **Braces**: Allman style (opening brace on a new line) for all blocks
- **Line length**: No hard limit, but keep lines readable (typically < 100 columns)

To format with a consistent style, you could create a `.clang-format` based on the existing code, but do not add one unless requested.

## Testing

The project contains an integrated test in the `main()` function. Running the compiled binary executes the self‑test.

### Run the test
```bash
./nor_log
```

The test performs an infinite loop that writes log entries and verifies addresses. It uses `assert()` for validation; if any assertion fails the program will abort. You can interrupt the test with Ctrl+C.

### Adding Unit Tests
If you need to add new unit tests, consider extending the existing `main()` test or creating a separate test file. Follow the pattern of setting up `flash_ram`, initializing `nor_log_ctx_t`, and calling the API functions.

## Code Style Guidelines

### Naming Conventions
- **Structs/typedefs**: Suffix with `_t` (e.g., `base_log_entry_t`, `nor_log_ctx_t`)
- **Variables**: `snake_case`
- **Macros**: `UPPER_SNAKE_CASE`
- **Functions**: Prefix with `nor_log_` for public API, `static` for internal helpers

### Header Includes
Include standard headers in this order:
1. System headers (`<stdint.h>`, `<assert.h>`, etc.)
2. Project‑specific headers (none currently)

Example:
```c
#include <stdint.h>
#include <assert.h>
#include <stdbool.h>
#include <string.h>
```

### Types
- Use `stdint.h` types (`uint32_t`, `uint16_t`, etc.) for fixed‑width integers.
- Use `bool` from `stdbool.h` for Boolean values.
- Avoid `int`, `long`, etc. unless required by an external API.

### Macros
- Use `#define` for constants and inline functions where appropriate.
- Multi‑statement macros must be wrapped in a `do { … } while (0)` block.
- Parenthesize macro arguments and the whole expansion.

Example:
```c
#define FLASH_WRITE(addr, buf, len)                       \
    do                                                    \
    {                                                     \
        /* implementation */                              \
    } while (0)
```

### Functions
- Small, performance‑critical helper functions should be declared `static inline`.
- Public API functions are declared without `static`.
- Function definitions have the return type on the same line as the function name, with the opening brace on a new line:
```c
void nor_log_init_next_entry_addr(nor_log_ctx_t *ctx)
{
    // ...
}
```

### Error Handling
- Use `assert()` for invariants and debugging checks.
- The code does not currently have a runtime error‑reporting mechanism; failures are considered unrecoverable.

### Comments
- **Avoid unnecessary comments.** The commit history shows a deliberate removal of comments.
- Code should be self‑documenting through clear naming and structure.
- If a comment is essential (e.g., explaining a non‑obvious algorithm), keep it concise.

### Struct and Union Layout
- Place opening braces on a new line for structs/unions.
- Indent member declarations with 4 spaces.
- Align members for readability (as seen in `flash_ram`).

Example:
```c
union
{
    struct
    {
        uint32_t id;
        uint32_t data[15];
    } log_entry[4];
    // ...
} flash_ram;
```

## Commit Message Convention

Follow **Conventional Commits** as observed in the git history:

- `feat:` – a new feature
- `fix:` – a bug fix
- `refactor:` – code change that neither fixes a bug nor adds a feature
- `docs:` – documentation only changes
- `style:` – changes that do not affect the meaning (white‑space, formatting, etc.)
- `test:` – adding or correcting tests
- `chore:` – maintenance tasks (build, tooling, etc.)

Start the subject line with a lowercase type, followed by a colon and a space. Keep the subject line under 50 characters. Optionally add a body separated by a blank line.

Example:
```
fix: initialize entry array to zero for consistent CRC
```

## Cursor / Copilot Rules

No `.cursorrules` or `.github/copilot-instructions.md` files exist. If you create them, ensure they align with the guidelines above.

## Agent Workflow

1. **Understand the scope** – The project is small and focused; changes should stay within the single source file unless explicitly requested.
2. **Respect the existing style** – Match indentation, naming, and formatting exactly.
3. **Run the test** – After any modification, compile and execute `./nor_log` to verify the self‑test still passes.
4. **Check for warnings** – Compile with `-Wall -Wextra -Werror` to ensure no new warnings are introduced.
5. **Commit with care** – Use the Conventional Commits format and keep commits focused.

## Known Issues / Limitations

- The `main()` test runs an infinite loop (`for (size_t i = 0; i < UINT32_MAX; i++)`). It will never terminate under normal execution.
- The CRC16 implementation is a stub (`#define CRC16(data, len) (0xcc16)`). A real implementation would need to be supplied by the user.
- The flash emulation (`flash_write`/`flash_read`) assumes a specific memory layout; adapt it to the target hardware.

---

*This document is intended for AI agents and should be updated when the project’s build process or style conventions change.*
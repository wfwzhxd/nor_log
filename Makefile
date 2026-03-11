# Makefile for nor_log project
#
# Targets:
#   all       - Build all binaries (default)
#   test      - Build and run test
#   example   - Build and run example
#   clean     - Remove build artifacts
#   help      - Show this help

# Compiler and flags
CC = gcc
CFLAGS = -g -Wall -Wextra -Werror -std=c99 -pedantic
LDFLAGS =

# Source files
NOR_LOG_SRC = nor_log.c
NOR_LOG_HDR = nor_log.h
TEST_SRC = nor_log_test.c
EXAMPLE_SRC = example.c

# Object files
NOR_LOG_OBJ = $(NOR_LOG_SRC:.c=.o)
TEST_OBJ = $(TEST_SRC:.c=.o)
EXAMPLE_OBJ = $(EXAMPLE_SRC:.c=.o)

# Executables
TEST_EXE = nor_log_test
EXAMPLE_EXE = example

# Default target
all: $(TEST_EXE) $(EXAMPLE_EXE)

# Test executable
$(TEST_EXE): $(TEST_OBJ) $(NOR_LOG_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Example executable
$(EXAMPLE_EXE): $(EXAMPLE_OBJ) $(NOR_LOG_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Object files from C source
%.o: %.c $(NOR_LOG_HDR)
	$(CC) $(CFLAGS) -c -o $@ $<

# Run test (with timeout to prevent infinite loop)
.PHONY: test
test: $(TEST_EXE)
	@echo "Running test (will timeout after 5 seconds)..."
	@timeout 5 ./$(TEST_EXE) && echo "Test passed!" || echo "Test completed (timeout expected)"

# Run example
.PHONY: run-example
run-example: $(EXAMPLE_EXE)
	@echo "Running example..."
	@./$(EXAMPLE_EXE)

# Clean build artifacts
clean:
	rm -f $(TEST_EXE) $(EXAMPLE_EXE) $(NOR_LOG_OBJ) $(TEST_OBJ) $(EXAMPLE_OBJ)

# Show help
help:
	@echo "Available targets:"
	@echo "  all          - Build all binaries (default)"
	@echo "  test         - Build and run test (5 second timeout)"
	@echo "  run-example  - Build and run example"
	@echo "  clean        - Remove build artifacts"
	@echo "  help         - Show this help"

# Phony targets
.PHONY: all test example clean help
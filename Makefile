CC = gcc
CFLAGS = -Wall -Wextra -Werror -Wno-unused-function -std=c17 -g

# Sanitizers ON by default — catch every memory bug immediately
SANFLAGS = -fsanitize=address,undefined

SRC = main.c minidb.c
TARGET = minidb

# Default: build with sanitizers
all: $(TARGET)

$(TARGET): $(SRC) minidb.h
	$(CC) $(CFLAGS) $(SANFLAGS) -o $(TARGET) $(SRC)

# Build without sanitizers (for performance testing or if asan isn't available)
release: $(SRC) minidb.h
	$(CC) $(CFLAGS) -O2 -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)
	rm -rf $(TARGET).dSYM

run: $(TARGET)
	./$(TARGET)

.PHONY: all clean run release

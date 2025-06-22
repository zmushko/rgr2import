# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2
LIBS = -lcurl -lcjson

# Target executable
TARGET = rgr2import

# Source files
SOURCES = main.c

# Object files
OBJECTS = $(SOURCES:.c=.o)

# Default target
all: $(TARGET)

# Build the main executable
$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(TARGET) $(LIBS)

# Compile source files to object files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean build files
clean:
	rm -f $(OBJECTS) $(TARGET)

# Install dependencies (for Debian/Ubuntu/Raspberry Pi OS)
deps:
	sudo apt-get update
	sudo apt-get install libcurl4-openssl-dev libcjson-dev

# Run the program
run: $(TARGET)
	./$(TARGET)

# Debug build
debug: CFLAGS += -g -DDEBUG
debug: $(TARGET)

# Phony targets
.PHONY: all clean deps run debug
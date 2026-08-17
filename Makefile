# Compiler settings
CC = gcc
CFLAGS = -O3 -Wall -Wextra
LDFLAGS = -lm

# Command name
TARGET = ascify
SRC = ascify.c

# Installation directory (Standard for Arch Linux manual installs)
PREFIX = /usr/local

.PHONY: all clean install uninstall

# Default target builds the binary
all: $(TARGET)

# Compile the binary (requires stb_image.h)
$(TARGET): $(SRC) stb_image.h
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET) $(LDFLAGS)

# Automatically download stb_image.h if missing
stb_image.h:
	curl -O https://raw.githubusercontent.com/nothings/stb/master/stb_image.h

# Install binary to system PATH
install: $(TARGET)
	@echo "Installing $(TARGET) to $(PREFIX)/bin..."
	install -Dm755 $(TARGET) $(PREFIX)/bin/$(TARGET)
	@echo "Installation complete! You can now use the '$(TARGET)' command."

# Remove the binary from the system
uninstall:
	@echo "Removing $(TARGET) from $(PREFIX)/bin..."
	rm -f $(PREFIX)/bin/$(TARGET)
	@echo "Uninstalled successfully."

# Clean up local build files
clean:
	rm -f $(TARGET) stb_image.h

# CometBuster Makefile with WAD Audio System
# Based on zenamp project structure
# Supports Linux and Windows builds with Miniz ZIP library

# Package information
PACKAGE_NAME = cometbuster
VERSION = 1.0.0

# Installation directories
PREFIX = /usr
BINDIR = $(PREFIX)/bin
DATADIR = $(PREFIX)/share
DOCDIR = $(DATADIR)/doc/$(PACKAGE_NAME)

# Compiler settings
CXX_LINUX = g++
CC_LINUX = gcc
CXX_WIN = x86_64-w64-mingw32-g++
CC_WIN = x86_64-w64-mingw32-gcc

# Common flags
CXXFLAGS_COMMON = -Wall -Wextra -g -std=c++11 -fpermissive
CFLAGS_COMMON = -Wall -Wextra -g

# Platform-specific package config commands
PKG_CONFIG_LINUX = pkg-config
PKG_CONFIG_WIN = mingw64-pkg-config
SDL2_CONFIG_LINUX = sdl2-config
SDL2_CONFIG_WIN = mingw64-sdl2-config

# Linux-specific flags
SDL2_CFLAGS_LINUX := $(shell $(SDL2_CONFIG_LINUX) --cflags 2>/dev/null || echo "")
SDL2_LIBS_LINUX := $(shell $(SDL2_CONFIG_LINUX) --libs 2>/dev/null || echo "-lSDL2")
GTK_CFLAGS_LINUX := $(shell $(PKG_CONFIG_LINUX) --cflags gtk+-3.0 2>/dev/null || echo "")
GTK_LIBS_LINUX := $(shell $(PKG_CONFIG_LINUX) --libs gtk+-3.0 2>/dev/null || echo "")
SDL2_MIXER_CFLAGS_LINUX := $(shell $(PKG_CONFIG_LINUX) --cflags SDL2_mixer 2>/dev/null || echo "")
SDL2_MIXER_LIBS_LINUX := $(shell $(PKG_CONFIG_LINUX) --libs SDL2_mixer 2>/dev/null || echo "-lSDL2_mixer")

CXXFLAGS_LINUX = $(CXXFLAGS_COMMON) $(SDL2_CFLAGS_LINUX) $(GTK_CFLAGS_LINUX) $(SDL2_MIXER_CFLAGS_LINUX) -DExternalSound -DLINUX -DVERSION=\"$(VERSION)\"
CFLAGS_LINUX = $(CFLAGS_COMMON) $(SDL2_CFLAGS_LINUX) $(GTK_CFLAGS_LINUX) $(SDL2_MIXER_CFLAGS_LINUX) -DExternalSound -DLINUX
LDFLAGS_LINUX = $(SDL2_LIBS_LINUX) $(GTK_LIBS_LINUX) $(SDL2_MIXER_LIBS_LINUX) -lm -pthread -lstdc++

# Optimization flags
CXXFLAGS_LINUX += -O2 -ffunction-sections -fdata-sections -flto
LDFLAGS_LINUX += -s -Wl,--gc-sections -flto

# Windows-specific flags
SDL2_CFLAGS_WIN := $(shell $(PKG_CONFIG_WIN) --cflags sdl2 2>/dev/null || echo "")
SDL2_LIBS_WIN := $(shell $(PKG_CONFIG_WIN) --libs sdl2 2>/dev/null || echo "-lSDL2")
GTK_CFLAGS_WIN := $(shell $(PKG_CONFIG_WIN) --cflags gtk+-3.0 2>/dev/null || echo "")
GTK_LIBS_WIN := $(shell $(PKG_CONFIG_WIN) --libs gtk+-3.0 2>/dev/null || echo "")
SDL2_MIXER_CFLAGS_WIN := $(shell $(PKG_CONFIG_WIN) --cflags SDL2_mixer 2>/dev/null || echo "")
SDL2_MIXER_LIBS_WIN := $(shell $(PKG_CONFIG_WIN) --libs SDL2_mixer 2>/dev/null || echo "-lSDL2_mixer")

CXXFLAGS_WIN = $(CXXFLAGS_COMMON) $(SDL2_CFLAGS_WIN) $(GTK_CFLAGS_WIN) $(SDL2_MIXER_CFLAGS_WIN) -DExternalSound -DWIN32 -D_WIN32 -DVERSION=\"$(VERSION)\"
CFLAGS_WIN = $(CFLAGS_COMMON) $(SDL2_CFLAGS_WIN) $(GTK_CFLAGS_WIN) $(SDL2_MIXER_CFLAGS_WIN) -DExternalSound -DWIN32 -D_WIN32
LDFLAGS_WIN = $(SDL2_LIBS_WIN) $(GTK_LIBS_WIN) $(SDL2_MIXER_LIBS_WIN) -lm -lstdc++ -lwinmm

CXXFLAGS_WIN += -O2 -ffunction-sections -fdata-sections
LDFLAGS_WIN += -s -Wl,--gc-sections

# Debug flags
DEBUG_FLAGS = -DDEBUG
CXXFLAGS_LINUX_DEBUG = $(CXXFLAGS_LINUX) $(DEBUG_FLAGS)
CFLAGS_LINUX_DEBUG = $(CFLAGS_LINUX) $(DEBUG_FLAGS)
CXXFLAGS_WIN_DEBUG = $(CXXFLAGS_WIN) $(DEBUG_FLAGS)
CFLAGS_WIN_DEBUG = $(CFLAGS_WIN) $(DEBUG_FLAGS)

# Source files - Game code
SOURCES_CPP_COMMON = comet_main.cpp cometbuster.cpp wad.cpp audio_wad.cpp cometbuster_spawn.cpp cometbuster_init.cpp

# Source files - Miniz WAD system (C files)
SOURCES_C = miniz.c miniz_tdef.c miniz_tinfl.c miniz_zip.c

# Object files for different platforms
OBJECTS_CPP_LINUX = $(SOURCES_CPP_COMMON:.cpp=.o)
OBJECTS_C_LINUX = $(SOURCES_C:.c=.o)
OBJECTS_LINUX = $(OBJECTS_CPP_LINUX) $(OBJECTS_C_LINUX)

OBJECTS_CPP_WIN = $(SOURCES_CPP_COMMON:.cpp=.win.o)
OBJECTS_C_WIN = $(SOURCES_C:.c=.win.o)
OBJECTS_WIN = $(OBJECTS_CPP_WIN) $(OBJECTS_C_WIN)

OBJECTS_CPP_LINUX_DEBUG = $(SOURCES_CPP_COMMON:.cpp=.debug.o)
OBJECTS_C_LINUX_DEBUG = $(SOURCES_C:.c=.debug.o)
OBJECTS_LINUX_DEBUG = $(OBJECTS_CPP_LINUX_DEBUG) $(OBJECTS_C_LINUX_DEBUG)

OBJECTS_CPP_WIN_DEBUG = $(SOURCES_CPP_COMMON:.cpp=.win.debug.o)
OBJECTS_C_WIN_DEBUG = $(SOURCES_C:.c=.win.debug.o)
OBJECTS_WIN_DEBUG = $(OBJECTS_CPP_WIN_DEBUG) $(OBJECTS_C_WIN_DEBUG)

# Target executables
EXECUTABLE_LINUX = cometbuster
EXECUTABLE_WIN = cometbuster.exe
EXECUTABLE_LINUX_DEBUG = cometbuster_debug
EXECUTABLE_WIN_DEBUG = cometbuster_debug.exe

# Build directories
BUILD_DIR = build
BUILD_DIR_LINUX = $(BUILD_DIR)/linux
BUILD_DIR_WIN = $(BUILD_DIR)/windows
BUILD_DIR_LINUX_DEBUG = $(BUILD_DIR)/linux_debug
BUILD_DIR_WIN_DEBUG = $(BUILD_DIR)/windows_debug

# Extra objects for linking
EXTRA_OBJS ?=

# Create necessary directories
$(shell mkdir -p $(BUILD_DIR_LINUX) $(BUILD_DIR_WIN) $(BUILD_DIR_LINUX_DEBUG) $(BUILD_DIR_WIN_DEBUG))

# Default target - build for Linux
.PHONY: all
all: linux

# OS-specific builds
.PHONY: windows
windows: cometbuster-windows

.PHONY: linux
linux: cometbuster-linux

# Debug targets
.PHONY: debug
debug: cometbuster-linux-debug cometbuster-windows-debug

#
# Linux build targets
#
.PHONY: cometbuster-linux
cometbuster-linux: $(BUILD_DIR_LINUX)/$(EXECUTABLE_LINUX)

$(BUILD_DIR_LINUX)/$(EXECUTABLE_LINUX): $(addprefix $(BUILD_DIR_LINUX)/,$(OBJECTS_LINUX)) $(EXTRA_OBJS)
	@echo "Linking Linux executable: $@"
	$(CXX_LINUX) $(CXXFLAGS_LINUX) $(addprefix $(BUILD_DIR_LINUX)/,$(OBJECTS_LINUX)) $(EXTRA_OBJS) -o $@ $(LDFLAGS_LINUX)
	@echo "✓ Build complete: $@"

# Linux compilation rules - C++ files
$(BUILD_DIR_LINUX)/%.o: %.cpp
	@echo "Compiling (Linux): $<"
	$(CXX_LINUX) $(CXXFLAGS_LINUX) -c $< -o $@

# Linux compilation rules - C files
$(BUILD_DIR_LINUX)/%.o: %.c
	@echo "Compiling (Linux): $<"
	$(CC_LINUX) $(CFLAGS_LINUX) -c $< -o $@

#
# Linux debug targets
#
.PHONY: cometbuster-linux-debug
cometbuster-linux-debug: $(BUILD_DIR_LINUX_DEBUG)/$(EXECUTABLE_LINUX_DEBUG)

$(BUILD_DIR_LINUX_DEBUG)/$(EXECUTABLE_LINUX_DEBUG): $(addprefix $(BUILD_DIR_LINUX_DEBUG)/,$(OBJECTS_LINUX_DEBUG)) $(EXTRA_OBJS)
	@echo "Linking Linux debug executable: $@"
	$(CXX_LINUX) $(CXXFLAGS_LINUX_DEBUG) $(addprefix $(BUILD_DIR_LINUX_DEBUG)/,$(OBJECTS_LINUX_DEBUG)) $(EXTRA_OBJS) -o $@ $(LDFLAGS_LINUX)
	@echo "✓ Debug build complete: $@"

# Linux debug compilation rules - C++ files
$(BUILD_DIR_LINUX_DEBUG)/%.debug.o: %.cpp
	@echo "Compiling (Linux Debug): $<"
	$(CXX_LINUX) $(CXXFLAGS_LINUX_DEBUG) -c $< -o $@

# Linux debug compilation rules - C files
$(BUILD_DIR_LINUX_DEBUG)/%.debug.o: %.c
	@echo "Compiling (Linux Debug): $<"
	$(CC_LINUX) $(CFLAGS_LINUX_DEBUG) -c $< -o $@

#
# Windows build targets
#
.PHONY: cometbuster-windows
cometbuster-windows: $(BUILD_DIR_WIN)/$(EXECUTABLE_WIN)

$(BUILD_DIR_WIN)/$(EXECUTABLE_WIN): $(addprefix $(BUILD_DIR_WIN)/,$(OBJECTS_WIN)) $(EXTRA_OBJS)
	@echo "Linking Windows executable: $@"
	$(CXX_WIN) $(CXXFLAGS_WIN) $(addprefix $(BUILD_DIR_WIN)/,$(OBJECTS_WIN)) $(EXTRA_OBJS) -o $@ $(LDFLAGS_WIN)
	@echo "✓ Windows build complete: $@"

# Windows compilation rules - C++ files
$(BUILD_DIR_WIN)/%.win.o: %.cpp
	@echo "Compiling (Windows): $<"
	$(CXX_WIN) $(CXXFLAGS_WIN) -c $< -o $@

# Windows compilation rules - C files
$(BUILD_DIR_WIN)/%.win.o: %.c
	@echo "Compiling (Windows): $<"
	$(CC_WIN) $(CFLAGS_WIN) -c $< -o $@

#
# Windows debug targets
#
.PHONY: cometbuster-windows-debug
cometbuster-windows-debug: $(BUILD_DIR_WIN_DEBUG)/$(EXECUTABLE_WIN_DEBUG)

$(BUILD_DIR_WIN_DEBUG)/$(EXECUTABLE_WIN_DEBUG): $(addprefix $(BUILD_DIR_WIN_DEBUG)/,$(OBJECTS_WIN_DEBUG)) $(EXTRA_OBJS)
	@echo "Linking Windows debug executable: $@"
	$(CXX_WIN) $(CXXFLAGS_WIN_DEBUG) $(addprefix $(BUILD_DIR_WIN_DEBUG)/,$(OBJECTS_WIN_DEBUG)) $(EXTRA_OBJS) -o $@ $(LDFLAGS_WIN)
	@echo "✓ Windows debug build complete: $@"

# Windows debug compilation rules - C++ files
$(BUILD_DIR_WIN_DEBUG)/%.win.debug.o: %.cpp
	@echo "Compiling (Windows Debug): $<"
	$(CXX_WIN) $(CXXFLAGS_WIN_DEBUG) -c $< -o $@

# Windows debug compilation rules - C files
$(BUILD_DIR_WIN_DEBUG)/%.win.debug.o: %.c
	@echo "Compiling (Windows Debug): $<"
	$(CC_WIN) $(CFLAGS_WIN_DEBUG) -c $< -o $@

# Install target (Linux only)
.PHONY: install
install: $(BUILD_DIR_LINUX)/$(EXECUTABLE_LINUX)
	@echo "Installing $(EXECUTABLE_LINUX)..."
	install -D -m 755 $(BUILD_DIR_LINUX)/$(EXECUTABLE_LINUX) $(DESTDIR)$(BINDIR)/$(EXECUTABLE_LINUX)
	@if [ -f README.md ]; then \
		install -D -m 644 README.md $(DESTDIR)$(DOCDIR)/README.md; \
	fi
	@if [ -f cometbuster.wad ]; then \
		install -D -m 644 cometbuster.wad $(DESTDIR)$(DATADIR)/$(PACKAGE_NAME)/cometbuster.wad; \
		echo "Installed WAD file to $(DATADIR)/$(PACKAGE_NAME)/"; \
	fi
	@echo "✓ Installation complete"

# Uninstall target
.PHONY: uninstall
uninstall:
	@echo "Uninstalling $(EXECUTABLE_LINUX)..."
	rm -f $(DESTDIR)$(BINDIR)/$(EXECUTABLE_LINUX)
	rm -rf $(DESTDIR)$(DOCDIR)
	rm -f $(DESTDIR)$(DATADIR)/$(PACKAGE_NAME)/cometbuster.wad
	@echo "✓ Uninstall complete"

# Clean target
.PHONY: clean
clean:
	@echo "Cleaning build artifacts..."
	find $(BUILD_DIR) -type f -name "*.o" -delete 2>/dev/null || true
	find $(BUILD_DIR) -type f -name "*.exe" -delete 2>/dev/null || true
	rm -f $(BUILD_DIR_LINUX)/$(EXECUTABLE_LINUX)
	rm -f $(BUILD_DIR_LINUX_DEBUG)/$(EXECUTABLE_LINUX_DEBUG)
	rm -f $(BUILD_DIR_WIN)/$(EXECUTABLE_WIN)
	rm -f $(BUILD_DIR_WIN_DEBUG)/$(EXECUTABLE_WIN_DEBUG)
	@echo "✓ Clean complete"

# Clean all build directories
.PHONY: clean-all
clean-all: clean
	@echo "Removing build directories..."
	rm -rf $(BUILD_DIR)
	@echo "✓ Clean all complete"

# WAD file creation
.PHONY: wad
wad:
	@if [ -d cometbuster_sounds/sounds ]; then \
		echo "Creating WAD file..."; \
		python3 create_wad.py; \
		echo "✓ WAD file created: cometbuster.wad"; \
	else \
		echo "Error: cometbuster_sounds/sounds directory not found!"; \
		echo "Create the directory and add MP3 files, then run: make wad"; \
		exit 1; \
	fi

# Help target
.PHONY: help
help:
	@echo "CometBuster Makefile - Available targets:"
	@echo ""
	@echo "Build targets:"
	@echo "  make               - Build for Linux (default)"
	@echo "  make linux         - Build for Linux"
	@echo "  make windows       - Build for Windows"
	@echo "  make debug         - Build debug versions for both platforms"
	@echo "  make cometbuster-linux-debug   - Build debug for Linux"
	@echo "  make cometbuster-windows-debug - Build debug for Windows"
	@echo ""
	@echo "WAD audio targets:"
	@echo "  make wad           - Create cometbuster.wad from sound files"
	@echo ""
	@echo "Installation (Linux only):"
	@echo "  make install       - Install to $(PREFIX)"
	@echo "  make uninstall     - Remove installed files"
	@echo ""
	@echo "Cleanup targets:"
	@echo "  make clean         - Remove all object files and executables"
	@echo "  make clean-all     - Remove all build directories"
	@echo ""
	@echo "Help:"
	@echo "  make help          - Show this help message"
	@echo ""
	@echo "Build outputs:"
	@echo "  Linux:       $(BUILD_DIR_LINUX)/$(EXECUTABLE_LINUX)"
	@echo "  Windows:     $(BUILD_DIR_WIN)/$(EXECUTABLE_WIN)"
	@echo "  Debug Linux: $(BUILD_DIR_LINUX_DEBUG)/$(EXECUTABLE_LINUX_DEBUG)"
	@echo "  Debug Win:   $(BUILD_DIR_WIN_DEBUG)/$(EXECUTABLE_WIN_DEBUG)"
	@echo ""
	@echo "Audio System:"
	@echo "  - Uses Miniz library for ZIP reading"
	@echo "  - SDL2_mixer for audio playback"
	@echo "  - Create sounds/ directory with MP3 files"
	@echo "  - Run: python3 create_wad.py"
	@echo "  - Distributes: binary + cometbuster.wad"
	@echo ""
	@echo "Example workflow:"
	@echo "  1. make linux                 # Build for Linux"
	@echo "  2. make wad                   # Create WAD from sounds"
	@echo "  3. ./build/linux/cometbuster  # Run the game"
	@echo ""

.PHONY: all clean install uninstall help linux windows debug wad

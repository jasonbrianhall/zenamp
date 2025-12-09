# Enhanced Makefile with Windows and Linux Support
# Based on zenamp project structure

# Package information for RPM
PACKAGE_NAME = zenamp
GIT_COUNT := $(shell git rev-list --count HEAD 2>/dev/null || echo 0)

# Allow VERSION to be overridden from command line
# Usage: make rpm VERSION=1.2.3
# VERSION is used for RPM packaging (format: 1.0.X)
# BUILDNUM is used by the application (format: X - just the number part)
ifndef VERSION
VERSION := 1.0.$(GIT_COUNT)
BUILDNUM := $(GIT_COUNT)
else
# Extract BUILDNUM from VERSION by removing "1.0." prefix
BUILDNUM := $(patsubst 1.0.%,%,$(VERSION))
endif

RELEASE = 1
ARCH = x86_64

# Installation directories
PREFIX = /usr
BINDIR = $(PREFIX)/bin
DATADIR = $(PREFIX)/share
DOCDIR = $(DATADIR)/doc/$(PACKAGE_NAME)
MANDIR = $(DATADIR)/man/man1
ICONDIR = $(DATADIR)/icons/hicolor
APPDIR = $(DATADIR)/applications

# RPM Build directories
RPM_TOPDIR = $(HOME)/rpmbuild
RPM_BUILDROOT = $(RPM_TOPDIR)/BUILDROOT/$(PACKAGE_NAME)-$(VERSION)-$(RELEASE).$(ARCH)

# Compiler settings
CXX_LINUX = g++
CC_LINUX = gcc
CXX_WIN = x86_64-w64-mingw32-g++
CC_WIN = x86_64-w64-mingw32-gcc
WINDRES = x86_64-w64-mingw32-windres

# Common flags
CXXFLAGS_COMMON = -Wall -Wextra -g -fpermissive
CFLAGS_COMMON = -Wall -Wextra -g

# Debug flags
DEBUG_FLAGS = -DDEBUG

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
VORBIS_CFLAGS_LINUX := $(shell $(PKG_CONFIG_LINUX) --cflags vorbisfile 2>/dev/null || echo "")
VORBIS_LIBS_LINUX := $(shell $(PKG_CONFIG_LINUX) --libs vorbisfile 2>/dev/null || echo "")
OPUS_CFLAGS_LINUX := $(shell $(PKG_CONFIG_LINUX) --cflags opusfile 2>/dev/null || echo "-I/usr/include/opus")
OPUS_LIBS_LINUX := $(shell $(PKG_CONFIG_LINUX) --libs opusfile 2>/dev/null || echo "-lopusfile -lopus")

# Linux-specific libraries
AVCODEC_CFLAGS_LINUX := $(shell $(PKG_CONFIG_LINUX) --cflags libavcodec libavformat libavutil libswresample 2>/dev/null || echo "")
AVCODEC_LIBS_LINUX := $(shell $(PKG_CONFIG_LINUX) --libs libavcodec libavformat libavutil libswresample 2>/dev/null || echo "-lavcodec -lavformat -lavutil -lswresample")

CXXFLAGS_LINUX = $(CXXFLAGS_COMMON) $(SDL2_CFLAGS_LINUX) $(GTK_CFLAGS_LINUX) $(VORBIS_CFLAGS_LINUX) $(OPUS_CFLAGS_LINUX) $(AVCODEC_CFLAGS_LINUX) -DLINUX -DVERSION=\"$(BUILDNUM)\"
CFLAGS_LINUX = $(CFLAGS_COMMON) $(SDL2_CFLAGS_LINUX) $(GTK_CFLAGS_LINUX) $(VORBIS_CFLAGS_LINUX) $(OPUS_CFLAGS_LINUX) $(AVCODEC_CFLAGS_LINUX) -DLINUX
LDFLAGS_LINUX = $(SDL2_LIBS_LINUX) $(GTK_LIBS_LINUX) $(VORBIS_LIBS_LINUX) $(OPUS_LIBS_LINUX) $(AVCODEC_LIBS_LINUX) -lSDL2_mixer -lm -pthread -lstdc++ -lFLAC

CFLAGS_LINUX   += $(shell pkg-config --cflags taglib_c)
LDFLAGS_LINUX  += $(shell pkg-config --libs taglib_c)

CXXFLAGS_LINUX += -Os -ffunction-sections -fdata-sections -flto
LDFLAGS_LINUX  += -s -Wl,--gc-sections -flto


# Windows-specific flags
SDL2_CFLAGS_WIN := $(shell $(PKG_CONFIG_WIN) --cflags sdl2 2>/dev/null || echo "")
SDL2_LIBS_WIN := $(shell $(PKG_CONFIG_WIN) --libs sdl2 2>/dev/null || echo "-lSDL2")
GTK_CFLAGS_WIN := $(shell $(PKG_CONFIG_WIN) --cflags gtk+-3.0 2>/dev/null || echo "")
GTK_LIBS_WIN := $(shell $(PKG_CONFIG_WIN) --libs gtk+-3.0 2>/dev/null || echo "")
VORBIS_CFLAGS_WIN := $(shell $(PKG_CONFIG_WIN) --cflags vorbisfile 2>/dev/null || echo "")
VORBIS_LIBS_WIN := $(shell $(PKG_CONFIG_WIN) --libs vorbisfile 2>/dev/null || echo "")
OPUS_CFLAGS_WIN := $(shell $(PKG_CONFIG_WIN) --cflags opusfile 2>/dev/null || echo "")
OPUS_LIBS_WIN := $(shell $(PKG_CONFIG_WIN) --libs opusfile 2>/dev/null || echo "-lopusfile -lopus")

# Add TagLib support for Windows
TAGLIB_CFLAGS_WIN := $(shell $(PKG_CONFIG_WIN) --cflags taglib_c 2>/dev/null || echo "")
TAGLIB_LIBS_WIN := $(shell $(PKG_CONFIG_WIN) --libs taglib_c 2>/dev/null || echo "-ltag_c -ltag")

# Media Foundation libraries for M4A support on Windows
MEDIA_FOUNDATION_LIBS = -lmf -lmfplat -lmfreadwrite -lmfuuid -lole32

CXXFLAGS_WIN = $(CXXFLAGS_COMMON) $(SDL2_CFLAGS_WIN) $(GTK_CFLAGS_WIN) $(VORBIS_CFLAGS_WIN) $(OPUS_CFLAGS_WIN) $(TAGLIB_CFLAGS_WIN) -DWIN32 -D_WIN32 -DVERSION=\"$(BUILDNUM)\"
CFLAGS_WIN = $(CFLAGS_COMMON) $(SDL2_CFLAGS_WIN) $(GTK_CFLAGS_WIN) $(VORBIS_CFLAGS_WIN) $(OPUS_CFLAGS_WIN) $(TAGLIB_CFLAGS_WIN) -DWIN32 -D_WIN32
LDFLAGS_WIN = $(SDL2_LIBS_WIN) $(GTK_LIBS_WIN) $(VORBIS_LIBS_WIN) $(OPUS_LIBS_WIN) $(TAGLIB_LIBS_WIN) -lSDL2_mixer -lm -lstdc++ -lFLAC -lwinmm $(MEDIA_FOUNDATION_LIBS)

CXXFLAGS_WIN += -Os -ffunction-sections -fdata-sections
LDFLAGS_WIN  += -s -Wl,--gc-sections

# Debug-specific flags
CXXFLAGS_LINUX_DEBUG = $(CXXFLAGS_LINUX) $(DEBUG_FLAGS)
CFLAGS_LINUX_DEBUG = $(CFLAGS_LINUX) $(DEBUG_FLAGS)
CXXFLAGS_WIN_DEBUG = $(CXXFLAGS_WIN) $(DEBUG_FLAGS)
CFLAGS_WIN_DEBUG = $(CFLAGS_WIN) $(DEBUG_FLAGS)

# Common source files
SOURCES_CPP_COMMON = dbopl.cpp dbopl_wrapper.cpp main.cpp midiplayer.cpp \
	instruments.cpp virtual_mixer.cpp wav_converter.cpp audioconverter.cpp \
	convertmidi.cpp convertoggtowav.cpp convertopustowav.cpp convertmp3towav.cpp \
	convertflactowav.cpp vfs.cpp cache.cpp aiff.cpp keyboard.cpp \
	bubbles.cpp matrix.cpp fireworks.cpp dna.cpp dna2.cpp visualization.cpp \
	equalizer.cpp m3u.cpp help.cpp layout.cpp sudoku.cpp generatepuzzle.cpp \
	fourier.cpp ripples.cpp kaleidoscope.cpp bouncyball.cpp clock.cpp \
	robotchaser.cpp radialwave.cpp volume_meter.cpp drawbars.cpp \
	drawoscilloscope.cpp drawwaveform.cpp drawcircle.cpp blockstack.cpp \
	cdg.cpp karaoke.cpp zip_support.cpp metadata.cpp parrot.cpp sauron.cpp \
	hanoi.cpp beatchess.cpp beatcheckers.cpp queue.cpp drawfractalbloom.cpp \
	drawsymmetrycascade.cpp lrc2cdg.cpp drawtrippy.cpp drawwormhole.cpp \
	drawbd.cpp drawrabbithare.cpp audio_cache.cpp maze3d.cpp drawradialbars.cpp \
	icon.cpp bouncingcircle.cpp mandelbrot.cpp pong.cpp minesweeper.cpp cometbuster_spawn.cpp \
	cometbuster_init.cpp cometbuster_physics.cpp cometbuster_collision.cpp \
	cometbuster_boss.cpp cometbuster_score.cpp cometbuster_render.cpp \
	cometbuster_util.cpp

# Platform-specific source files
SOURCES_CPP_LINUX = $(SOURCES_CPP_COMMON) \
        convertm4atowavlin.cpp

SOURCES_CPP_WIN = $(SOURCES_CPP_COMMON) \
        convertm4atowavwin.cpp

SOURCES_C = miniz.c miniz_tdef.c miniz_tinfl.c miniz_zip.c

# Resource files for Windows
RESOURCE_FILE = icon.rc
RESOURCE_OBJ = icon.o

# Object files for different platforms
OBJECTS_CPP_LINUX = $(SOURCES_CPP_LINUX:.cpp=.o)
OBJECTS_C_LINUX = $(SOURCES_C:.c=.o)
OBJECTS_LINUX = $(OBJECTS_CPP_LINUX) $(OBJECTS_C_LINUX)

OBJECTS_CPP_WIN = $(SOURCES_CPP_WIN:.cpp=.win.o)
OBJECTS_C_WIN = $(SOURCES_C:.c=.win.o)
OBJECTS_WIN = $(OBJECTS_CPP_WIN) $(OBJECTS_C_WIN)

OBJECTS_CPP_LINUX_DEBUG = $(SOURCES_CPP_LINUX:.cpp=.debug.o)
OBJECTS_C_LINUX_DEBUG = $(SOURCES_C:.c=.debug.o)
OBJECTS_LINUX_DEBUG = $(OBJECTS_CPP_LINUX_DEBUG) $(OBJECTS_C_LINUX_DEBUG)

OBJECTS_CPP_WIN_DEBUG = $(SOURCES_CPP_WIN:.cpp=.win.debug.o)
OBJECTS_C_WIN_DEBUG = $(SOURCES_C:.c=.win.debug.o)
OBJECTS_WIN_DEBUG = $(OBJECTS_CPP_WIN_DEBUG) $(OBJECTS_C_WIN_DEBUG)

# Target executables
EXECUTABLE_LINUX = zenamp
EXECUTABLE_WIN = zenamp.exe
EXECUTABLE_LINUX_DEBUG = zenamp_debug
EXECUTABLE_WIN_DEBUG = zenamp_debug.exe

# Build directories
BUILD_DIR = build
BUILD_DIR_LINUX = $(BUILD_DIR)/linux
BUILD_DIR_WIN = $(BUILD_DIR)/windows
BUILD_DIR_LINUX_DEBUG = $(BUILD_DIR)/linux_debug
BUILD_DIR_WIN_DEBUG = $(BUILD_DIR)/windows_debug

# Windows DLL settings
DLL_SOURCE_DIR = /usr/x86_64-w64-mingw32/sys-root/mingw/bin

# Extra objects for linking (used by build.yml)
EXTRA_OBJS ?=

# Create necessary directories
$(shell mkdir -p $(BUILD_DIR_LINUX) $(BUILD_DIR_WIN) $(BUILD_DIR_LINUX_DEBUG) $(BUILD_DIR_WIN_DEBUG))

# Default target - build for Linux
.PHONY: all
all: linux

# OS-specific builds
.PHONY: windows
windows: zenamp-windows

.PHONY: linux
linux: zenamp-linux

# Debug targets
.PHONY: debug
debug: zenamp-linux-debug zenamp-windows-debug

#
# Linux build targets
#
.PHONY: zenamp-linux
zenamp-linux: $(BUILD_DIR_LINUX)/$(EXECUTABLE_LINUX)

$(BUILD_DIR_LINUX)/$(EXECUTABLE_LINUX): $(addprefix $(BUILD_DIR_LINUX)/,$(OBJECTS_LINUX))
	$(CXX_LINUX) $^ -o $@ $(LDFLAGS_LINUX)

# Linux compilation rules
$(BUILD_DIR_LINUX)/%.o: %.cpp
	$(CXX_LINUX) $(CXXFLAGS_LINUX) -c $< -o $@

$(BUILD_DIR_LINUX)/%.o: %.c
	$(CC_LINUX) $(CFLAGS_LINUX) -c $< -o $@

#
# Linux debug targets
#
.PHONY: zenamp-linux-debug
zenamp-linux-debug: $(BUILD_DIR_LINUX_DEBUG)/$(EXECUTABLE_LINUX_DEBUG)

$(BUILD_DIR_LINUX_DEBUG)/$(EXECUTABLE_LINUX_DEBUG): $(addprefix $(BUILD_DIR_LINUX_DEBUG)/,$(OBJECTS_LINUX_DEBUG))
	$(CXX_LINUX) $^ -o $@ $(LDFLAGS_LINUX)

# Linux debug compilation rules
$(BUILD_DIR_LINUX_DEBUG)/%.debug.o: %.cpp
	$(CXX_LINUX) $(CXXFLAGS_LINUX_DEBUG) -c $< -o $@

$(BUILD_DIR_LINUX_DEBUG)/%.debug.o: %.c
	$(CC_LINUX) $(CFLAGS_LINUX_DEBUG) -c $< -o $@

#
# Windows resource compilation
#
$(RESOURCE_OBJ): $(RESOURCE_FILE)
	@echo "Compiling Windows resources..."
	$(WINDRES) $(RESOURCE_FILE) -o $(RESOURCE_OBJ)

#
# Windows build targets
#
.PHONY: zenamp-windows
zenamp-windows: $(BUILD_DIR_WIN)/$(EXECUTABLE_WIN) zenamp-collect-dlls

$(BUILD_DIR_WIN)/$(EXECUTABLE_WIN): $(addprefix $(BUILD_DIR_WIN)/,$(OBJECTS_WIN)) $(EXTRA_OBJS)
	@if [ -f "$(RESOURCE_FILE)" ]; then \
		echo "Resource file found, compiling with embedded icon..."; \
		$(WINDRES) $(RESOURCE_FILE) -o $(RESOURCE_OBJ); \
		$(CXX_WIN) $(addprefix $(BUILD_DIR_WIN)/,$(OBJECTS_WIN)) $(RESOURCE_OBJ) $(EXTRA_OBJS) -o $@ $(LDFLAGS_WIN); \
	else \
		echo "No resource file found, compiling without embedded icon..."; \
		$(CXX_WIN) $(addprefix $(BUILD_DIR_WIN)/,$(OBJECTS_WIN)) $(EXTRA_OBJS) -o $@ $(LDFLAGS_WIN); \
	fi

# Windows compilation rules
$(BUILD_DIR_WIN)/%.win.o: %.cpp
	$(CXX_WIN) $(CXXFLAGS_WIN) -c $< -o $@

$(BUILD_DIR_WIN)/%.win.o: %.c
	$(CC_WIN) $(CFLAGS_WIN) -c $< -o $@

#
# Windows debug targets
#
.PHONY: zenamp-windows-debug
zenamp-windows-debug: $(BUILD_DIR_WIN_DEBUG)/$(EXECUTABLE_WIN_DEBUG) zenamp-collect-debug-dlls

$(BUILD_DIR_WIN_DEBUG)/$(EXECUTABLE_WIN_DEBUG): $(addprefix $(BUILD_DIR_WIN_DEBUG)/,$(OBJECTS_WIN_DEBUG)) $(EXTRA_OBJS)
	@if [ -f "$(RESOURCE_FILE)" ]; then \
		echo "Resource file found, compiling debug with embedded icon..."; \
		$(WINDRES) $(RESOURCE_FILE) -o $(RESOURCE_OBJ); \
		$(CXX_WIN) $(addprefix $(BUILD_DIR_WIN_DEBUG)/,$(OBJECTS_WIN_DEBUG)) $(RESOURCE_OBJ) $(EXTRA_OBJS) -o $@ $(LDFLAGS_WIN); \
	else \
		echo "No resource file found, compiling debug without embedded icon..."; \
		$(CXX_WIN) $(addprefix $(BUILD_DIR_WIN_DEBUG)/,$(OBJECTS_WIN_DEBUG)) $(EXTRA_OBJS) -o $@ $(LDFLAGS_WIN); \
	fi

# Windows debug compilation rules
$(BUILD_DIR_WIN_DEBUG)/%.win.debug.o: %.cpp
	$(CXX_WIN) $(CXXFLAGS_WIN_DEBUG) -c $< -o $@

$(BUILD_DIR_WIN_DEBUG)/%.win.debug.o: %.c
	$(CC_WIN) $(CFLAGS_WIN_DEBUG) -c $< -o $@

#
# DLL collection for Windows builds
#
.PHONY: zenamp-collect-dlls
zenamp-collect-dlls: $(BUILD_DIR_WIN)/$(EXECUTABLE_WIN)
	@echo "Collecting DLLs for zenamp..."
	@if [ -f $(BUILD_DIR_WIN)/collect_dlls.sh ]; then \
		$(BUILD_DIR_WIN)/collect_dlls.sh $(BUILD_DIR_WIN)/$(EXECUTABLE_WIN) $(DLL_SOURCE_DIR) $(BUILD_DIR_WIN); \
	else \
		echo "Warning: collect_dlls.sh not found. You may need to manually copy required DLLs."; \
		echo "Required DLLs typically include: SDL2.dll, libgtk-3-0.dll, libglib-2.0-0.dll, etc."; \
	fi

.PHONY: zenamp-collect-debug-dlls
zenamp-collect-debug-dlls: $(BUILD_DIR_WIN_DEBUG)/$(EXECUTABLE_WIN_DEBUG)
	@echo "Collecting Debug DLLs for zenamp..."
	@if [ -f $(BUILD_DIR_WIN)/collect_dlls.sh ]; then \
		$(BUILD_DIR_WIN)/collect_dlls.sh $(BUILD_DIR_WIN_DEBUG)/$(EXECUTABLE_WIN_DEBUG) $(DLL_SOURCE_DIR) $(BUILD_DIR_WIN_DEBUG); \
	else \
		echo "Warning: collect_dlls.sh not found. You may need to manually copy required DLLs."; \
	fi

# Install target (Linux only)
.PHONY: install
install: $(BUILD_DIR_LINUX)/$(EXECUTABLE_LINUX)
	install -D -m 755 $(BUILD_DIR_LINUX)/$(EXECUTABLE_LINUX) $(DESTDIR)$(BINDIR)/$(EXECUTABLE_LINUX)
	@if [ -f README.md ]; then \
		install -D -m 644 README.md $(DESTDIR)$(DOCDIR)/README.md; \
	fi
	@if [ -f LICENSE ]; then \
		install -D -m 644 LICENSE $(DESTDIR)$(DOCDIR)/LICENSE; \
	elif [ -f ../LICENSE.md ]; then \
		install -D -m 644 ../LICENSE.md $(DESTDIR)$(DOCDIR)/LICENSE; \
	fi
	@if [ -f $(PACKAGE_NAME).desktop ]; then \
		install -D -m 644 $(PACKAGE_NAME).desktop $(DESTDIR)$(APPDIR)/$(PACKAGE_NAME).desktop; \
	fi
	@if [ -f icon.png ]; then \
		install -D -m 644 icon.png $(DESTDIR)$(ICONDIR)/48x48/apps/$(PACKAGE_NAME).png; \
	elif [ -f $(PACKAGE_NAME).png ]; then \
		install -D -m 644 $(PACKAGE_NAME).png $(DESTDIR)$(ICONDIR)/48x48/apps/$(PACKAGE_NAME).png; \
	fi

# Uninstall target
.PHONY: uninstall
uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(EXECUTABLE_LINUX)
	rm -rf $(DESTDIR)$(DOCDIR)
	rm -f $(DESTDIR)$(APPDIR)/$(PACKAGE_NAME).desktop
	rm -f $(DESTDIR)$(ICONDIR)/48x48/apps/$(PACKAGE_NAME).png

#
# RPM Packaging targets
#
.PHONY: rpm
rpm: rpm-prep
	@echo "Building RPM package..."
	@if [ ! -f zenamp.spec ]; then \
		echo "Error: zenamp.spec file not found!"; \
		echo "Please create a zenamp.spec file in the current directory."; \
		exit 1; \
	fi
	@echo "Using version: $(VERSION)"
	@sed "s/@VERSION@/$(VERSION)/g" zenamp.spec > $(RPM_TOPDIR)/SPECS/zenamp.spec
	rpmbuild -ba $(RPM_TOPDIR)/SPECS/zenamp.spec

# Prepare RPM build environment
.PHONY: rpm-prep
rpm-prep:
	@echo "Setting up RPM build environment..."
	@mkdir -p $(RPM_TOPDIR)/{BUILD,RPMS,SOURCES,SPECS,SRPMS,BUILDROOT}
	@echo "Creating source tarball..."
	@mkdir -p /tmp/$(PACKAGE_NAME)-$(VERSION)
	@cp -r * /tmp/$(PACKAGE_NAME)-$(VERSION)/ 2>/dev/null || true
	@if [ -f ../LICENSE.md ]; then \
		cp ../LICENSE.md /tmp/$(PACKAGE_NAME)-$(VERSION)/LICENSE; \
	fi
	@tar czf $(RPM_TOPDIR)/SOURCES/$(PACKAGE_NAME)-$(VERSION).tar.gz -C /tmp $(PACKAGE_NAME)-$(VERSION)
	@rm -rf /tmp/$(PACKAGE_NAME)-$(VERSION)
	@echo "Source tarball created: $(RPM_TOPDIR)/SOURCES/$(PACKAGE_NAME)-$(VERSION).tar.gz"

# Build source RPM only
.PHONY: srpm
srpm: rpm-prep
	@echo "Building source RPM..."
	@if [ ! -f zenamp.spec ]; then \
		echo "Error: zenamp.spec file not found!"; \
		exit 1; \
	fi
	@echo "Using version: $(VERSION)"
	@sed "s/@VERSION@/$(VERSION)/g" zenamp.spec > $(RPM_TOPDIR)/SPECS/zenamp.spec
	rpmbuild -bs $(RPM_TOPDIR)/SPECS/zenamp.spec

# Clean RPM build artifacts
.PHONY: rpm-clean
rpm-clean:
	@echo "Cleaning RPM build artifacts..."
	rm -rf $(RPM_TOPDIR)

# Clean target
.PHONY: clean
clean:
	find $(BUILD_DIR) -type f -name "*.o" -delete 2>/dev/null || true
	find $(BUILD_DIR) -type f -name "*.dll" -delete 2>/dev/null || true
	find $(BUILD_DIR) -type f -name "*.exe" -delete 2>/dev/null || true
	rm -f $(BUILD_DIR_LINUX)/$(EXECUTABLE_LINUX)
	rm -f $(BUILD_DIR_LINUX_DEBUG)/$(EXECUTABLE_LINUX_DEBUG)
	rm -f $(BUILD_DIR_WIN)/$(EXECUTABLE_WIN)
	rm -f $(BUILD_DIR_WIN_DEBUG)/$(EXECUTABLE_WIN_DEBUG)
	rm -f $(RESOURCE_OBJ)

# Clean build directories
.PHONY: clean-all
clean-all: clean rpm-clean
	rm -rf $(BUILD_DIR)

# Help target
.PHONY: help
help:
	@echo "Available targets:"
	@echo "  make               - Build zenamp for Linux (default)"
	@echo "  make linux         - Build zenamp for Linux"
	@echo "  make windows       - Build zenamp for Windows (with icon if icon.rc exists)"
	@echo ""
	@echo "  make debug         - Build zenamp with debug symbols for both platforms"
	@echo "  make zenamp-linux-debug   - Build zenamp for Linux with debug symbols"
	@echo "  make zenamp-windows-debug - Build zenamp for Windows with debug symbols"
	@echo ""
	@echo "  make install       - Install zenamp to $(PREFIX) (Linux only)"
	@echo "  make uninstall     - Remove installed files"
	@echo ""
	@echo "  make rpm           - Create RPM package (requires zenamp.spec file)"
	@echo "  make rpm-prep      - Prepare RPM build environment and source tarball"
	@echo "  make srpm          - Build source RPM only"
	@echo "  make rpm-clean     - Clean RPM build artifacts"
	@echo ""
	@echo "  make clean         - Remove all build files"
	@echo "  make clean-all     - Remove all build files and directories (including RPM)"
	@echo "  make help          - Show this help message"
	@echo ""
	@echo "Build outputs:"
	@echo "  Linux:   $(BUILD_DIR_LINUX)/$(EXECUTABLE_LINUX)"
	@echo "  Windows: $(BUILD_DIR_WIN)/$(EXECUTABLE_WIN)"
	@echo "  RPM:     $(RPM_TOPDIR)/RPMS/$(ARCH)/$(PACKAGE_NAME)-$(VERSION)-$(RELEASE).$(ARCH).rpm"
	@echo "  Debug versions available in respective debug directories"
	@echo ""
	@echo "Package Information:"
	@echo "  Name:    $(PACKAGE_NAME)"
	@echo "  Version: $(VERSION)"
	@echo "  Release: $(RELEASE)"
	@echo ""
	@echo "Version Override:"
	@echo "  Use 'make rpm VERSION=x.y.z' to override the version number"
	@echo "  Example: make rpm VERSION=2.0.5"
	@echo "  Default: $(VERSION) (1.0.<git-commit-count>)"
	@echo ""
	@echo "Icon embedding:"
	@echo "  Windows builds will automatically include icon.rc if present"
	@echo "  Use EXTRA_OBJS=filename.o to include additional object files"
	@echo ""
	@echo "M4A Support:"
	@echo "  M4A/AAC audio files are supported on both platforms:"
	@echo "  - Windows: Uses Media Foundation (convertm4atowavwin.cpp)"
	@echo "  - Linux: Uses libavcodec/FFmpeg (convertm4atowavlin.cpp)"

.PHONY: all clean install uninstall help linux windows debug rpm rpm-prep srpm rpm-clean

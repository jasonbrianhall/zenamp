#!/bin/bash

# Help function
show_help() {
    echo "Usage: $0 EXE_PATH DLL_SOURCE_DIR OUTPUT_DIR"
    echo
    echo "Collects required DLLs for a Windows executable (GTK4 version)."
    echo
    echo "Parameters:"
    echo "  EXE_PATH        Path to the Windows executable"
    echo "  DLL_SOURCE_DIR  Directory containing source DLLs"
    echo "  OUTPUT_DIR      Directory where DLLs will be copied"
    exit 1
}

# Check parameters
if [ $# -ne 3 ]; then
    show_help
fi

EXE_PATH="$1"
DLL_SOURCE_DIR="$2"
OUTPUT_DIR="$3"

# Verify inputs
if [ ! -f "$EXE_PATH" ]; then
    echo "Error: Executable not found: $EXE_PATH"
    exit 1
fi

if [ ! -d "$DLL_SOURCE_DIR" ]; then
    echo "Error: DLL directory not found: $DLL_SOURCE_DIR"
    exit 1
fi

if [ ! -d "$OUTPUT_DIR" ]; then
    echo "Error: Output directory not found: $OUTPUT_DIR"
    exit 1
fi

# Derive GTK directory from DLL directory (typically /mingw64/bin -> /mingw64)
GTK_DIR=$(dirname "$DLL_SOURCE_DIR")

# Function to get direct dependencies of a file
get_dependencies() {
    local file="$1"
    objdump -p "$file" | grep "DLL Name:" | sed 's/\s*DLL Name: //'
}

# Function to check if a DLL exists in source directory
dll_exists() {
    local dll="$1"
    [ -f "$DLL_SOURCE_DIR/$dll" ]
}

# Initialize arrays for processing
declare -A processed_dlls
declare -a dlls_to_process
declare -a missing_dlls

# Create necessary GTK4 directories
echo "Creating GTK4 directory structure..."
mkdir -p "$OUTPUT_DIR/share/icons"
mkdir -p "$OUTPUT_DIR/share/themes"
mkdir -p "$OUTPUT_DIR/lib/gdk-pixbuf-2.0"
mkdir -p "$OUTPUT_DIR/lib/gtk-4.0"

# Copy GTK4 theme/icon files
echo "Copying GTK4 theme files..."

if [ -d "$GTK_DIR/share/icons/hicolor" ]; then
    cp -r "$GTK_DIR/share/icons/hicolor" "$OUTPUT_DIR/share/icons/"
fi

if [ -d "$GTK_DIR/share/themes/Adwaita" ]; then
    cp -r "$GTK_DIR/share/themes/Adwaita" "$OUTPUT_DIR/share/themes/"
fi

if [ -d "$GTK_DIR/share/themes/Adwaita-dark" ]; then
    cp -r "$GTK_DIR/share/themes/Adwaita-dark" "$OUTPUT_DIR/share/themes/"
fi

# Create GTK4 settings file
cat > "$OUTPUT_DIR/settings.ini" << EOF
[Settings]
gtk-theme-name = Adwaita
gtk-icon-theme-name = hicolor
gtk-font-name = Segoe UI 9
EOF

# ============================================================================
# GDK-PIXBUF LOADER MODULES (svg, png, jpeg, etc.)
# ============================================================================
# These are discovered by gdk-pixbuf at runtime via loaders.cache and loaded
# with dlopen()/LoadLibrary() - NOT through the exe's PE import table - so
# the objdump-based dependency walk above can never find them no matter how
# deep it recurses. Same root cause as the ANGLE DLLs earlier. Without this,
# gdk_pixbuf_loader_new_with_type("svg", NULL) (used for the chess piece
# sprites, among other things) returns NULL at runtime with no error set -
# it just silently fails. The "mkdir -p .../lib/gdk-pixbuf-2.0" above only
# ever created the parent folder; nothing was actually copying the loaders
# into it.
echo "Copying gdk-pixbuf loader modules..."
GDK_PIXBUF_DIR=$(find "$GTK_DIR/lib/gdk-pixbuf-2.0" -maxdepth 1 -type d -regex '.*/[0-9.]+' 2>/dev/null | sort -V | tail -1)

if [ -n "$GDK_PIXBUF_DIR" ] && [ -d "$GDK_PIXBUF_DIR/loaders" ]; then
    GDK_PIXBUF_VERSION=$(basename "$GDK_PIXBUF_DIR")
    OUTPUT_LOADERS_DIR="$OUTPUT_DIR/lib/gdk-pixbuf-2.0/$GDK_PIXBUF_VERSION/loaders"
    mkdir -p "$OUTPUT_LOADERS_DIR"

    for loader_dll in "$GDK_PIXBUF_DIR"/loaders/*.dll; do
        [ -f "$loader_dll" ] || continue
        cp "$loader_dll" "$OUTPUT_LOADERS_DIR/"
        echo "  Copied loader: $(basename "$loader_dll")"

        # Unlike the loader modules themselves, THEIR dependencies (e.g.
        # librsvg-2-2.dll for the SVG loader, libpng16-16.dll for the PNG
        # loader) are statically imported, so objdump *can* find these -
        # feed them into the existing flat-bin-dir processing queue so the
        # normal recursive walk picks up everything underneath them too.
        for subdep in $(get_dependencies "$loader_dll"); do
            if [ "${processed_dlls[$subdep]}" != "1" ]; then
                if dll_exists "$subdep"; then
                    dlls_to_process+=("$subdep")
                else
                    if [[ ! " ${missing_dlls[@]} " =~ " ${subdep} " ]]; then
                        missing_dlls+=("$subdep")
                    fi
                fi
            fi
        done
    done

    # loaders.cache tells gdk-pixbuf which physical path to LoadLibrary()
    # for each format - the build machine's cache has the sysroot's
    # absolute path baked in, which doesn't exist on the deployed Windows
    # machine. Rewrite each module path to be relative to the exe, matching
    # the tree just built above. This assumes the app is launched with its
    # own directory as the working directory (true for double-click and for
    # the zenamp_test-style layout used so far) - see the runtime env-var
    # fallback in main() for the case where that assumption doesn't hold.
    if [ -f "$GDK_PIXBUF_DIR/loaders.cache" ]; then
        sed -E "s#\"[^\"]*/loaders/([^/\"]+)\"#\"lib/gdk-pixbuf-2.0/$GDK_PIXBUF_VERSION/loaders/\1\"#g" \
            "$GDK_PIXBUF_DIR/loaders.cache" > "$OUTPUT_DIR/lib/gdk-pixbuf-2.0/$GDK_PIXBUF_VERSION/loaders.cache"
        echo "  Rewrote loaders.cache for the deployed layout"
    else
        echo "  WARNING: no loaders.cache found at $GDK_PIXBUF_DIR - loader DLLs were copied but gdk-pixbuf has no cache telling it they exist, so SVG/PNG/etc. loading will likely still fail at runtime. Generate one with gdk-pixbuf-query-loaders (the mingw-targeted build of it, run under Wine) against $GDK_PIXBUF_DIR, or rely on the GDK_PIXBUF_MODULEDIR runtime fallback instead."
    fi
else
    echo "  WARNING: no gdk-pixbuf-2.0/<version>/loaders directory found under $GTK_DIR - SVG piece sprites and any other pixbuf-loader-dependent image loading will fail at runtime on the deployed machine."
fi


initial_dlls=$(get_dependencies "$EXE_PATH")

# Add initial DLLs to processing queue
for dll in $initial_dlls; do
    if dll_exists "$dll"; then
        dlls_to_process+=("$dll")
    else
        missing_dlls+=("$dll")
    fi
done

# Add SDL3.dll if present
echo "Checking for SDL3.dll..."
if dll_exists "SDL3.dll"; then
    if [ "${processed_dlls[SDL3.dll]}" != "1" ]; then
        dlls_to_process+=("SDL3.dll")
    fi
else
    missing_dlls+=("SDL3.dll")
fi

# GTK4's GDK win32 backend loads its GL renderer (ANGLE) via libepoxy at
# runtime through LoadLibrary(), not through a static PE import - so neither
# the exe nor any DLL in the dependency chain ever references libEGL.dll or
# libGLESv2.dll in its import table, and the objdump-based walk above can
# never find them no matter how deep it recurses. Without them, GDK's GL
# context creation silently fails and GTK4 crashes the first time the
# window is realized (gtk_window_present()) - it does NOT fall back to
# cairo gracefully on its own. They must be added explicitly, same as the
# SDL3.dll case above.
echo "Checking for ANGLE (libEGL.dll / libGLESv2.dll)..."
for angle_dll in "libEGL.dll" "libGLESv2.dll"; do
    if dll_exists "$angle_dll"; then
        if [ "${processed_dlls[$angle_dll]}" != "1" ]; then
            dlls_to_process+=("$angle_dll")
        fi
    else
        missing_dlls+=("$angle_dll")
    fi
done

# NOTE on d3dcompiler_47.dll: ANGLE needs it (to compile its translated
# HLSL shaders), but it is deliberately NOT auto-bundled here. On a Linux
# build host the only copy that turns up under a generic search is often
# Wine's own reimplementation (used internally by Wine's D3D emulation),
# which is not a drop-in substitute for the real Microsoft shader compiler
# and would be actively wrong to ship - it's not just "missing," it would
# silently misbehave in a way that's much harder to diagnose than an
# absent DLL. Rely on the genuine copy already present on the target
# Windows machine (it ships with Windows 10/11 and the D3D/VC++
# redistributables) rather than trying to source one from the build
# environment. If a target machine somehow lacks it, get the real one
# from Microsoft's redistributable, not from a Wine install.

# Process DLLs recursively
while [ ${#dlls_to_process[@]} -gt 0 ]; do
    current_dll="${dlls_to_process[0]}"
    dlls_to_process=("${dlls_to_process[@]:1}")
    
    # Skip if already processed
    [ "${processed_dlls[$current_dll]}" = "1" ] && continue
    
    echo "Processing: $current_dll"
    
    # Mark as processed
    processed_dlls[$current_dll]=1
    
    # Copy DLL to output directory
    cp "$DLL_SOURCE_DIR/$current_dll" "$OUTPUT_DIR/"
    
    # Get dependencies of current DLL
    subdeps=$(get_dependencies "$DLL_SOURCE_DIR/$current_dll")
    
    # Add new dependencies to processing queue
    for dll in $subdeps; do
        if [ "${processed_dlls[$dll]}" != "1" ]; then
            if dll_exists "$dll"; then
                dlls_to_process+=("$dll")
            else
                if [[ ! " ${missing_dlls[@]} " =~ " ${dll} " ]]; then
                    missing_dlls+=("$dll")
                fi
            fi
        fi
    done
done

# Print summary
echo -e "\nDLL Collection Complete!"
echo "Copied ${#processed_dlls[@]} DLLs"

if [ ${#missing_dlls[@]} -gt 0 ]; then
    echo -e "\nNote: The following DLLs were not found in the source directory:"
    printf '%s\n' "${missing_dlls[@]}"
    echo "These are likely system DLLs that will be present on the target Windows system."
fi


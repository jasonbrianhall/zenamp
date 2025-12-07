#!/usr/bin/env python3
"""
CometBuster WAD File Creator

Creates a WAD file (ZIP archive) containing game audio files.
WAD files compress multiple audio files into a single distributable archive.

Usage:
    python3 create_wad.py
    python3 create_wad.py <input_dir> <output_file>
"""

import zipfile
import os
import sys
from pathlib import Path

def create_wad(sounds_dir, output_wad):
    """Create a WAD file (ZIP archive) from a sounds directory"""
    
    if not os.path.isdir(sounds_dir):
        print(f"❌ Error: Directory '{sounds_dir}' not found")
        sys.exit(1)
    
    # Create ZIP file with DEFLATE compression
    print(f"Creating WAD: {output_wad}")
    print(f"From directory: {sounds_dir}\n")
    
    total_original = 0
    total_compressed = 0
    
    with zipfile.ZipFile(output_wad, 'w', zipfile.ZIP_DEFLATED) as wad:
        for root, dirs, files in os.walk(sounds_dir):
            for file in sorted(files):
                file_path = os.path.join(root, file)
                # Archive name is relative path
                arcname = os.path.relpath(file_path, os.path.dirname(sounds_dir))
                
                file_size = os.path.getsize(file_path)
                total_original += file_size
                
                print(f"  Adding: {arcname} ({file_size:,} bytes)")
                wad.write(file_path, arcname)
    
    # Get compressed size
    compressed_size = os.path.getsize(output_wad)
    total_compressed = compressed_size
    
    print(f"\n✅ WAD file created successfully!")
    print(f"   Output: {output_wad}")
    print(f"   Original size: {total_original:,} bytes ({total_original/1024/1024:.2f} MB)")
    print(f"   Compressed size: {compressed_size:,} bytes ({compressed_size/1024/1024:.2f} MB)")
    
    if total_original > 0:
        ratio = (1.0 - compressed_size / total_original) * 100
        print(f"   Compression: {ratio:.1f}%")
    
    # List contents
    print(f"\n📦 WAD Contents ({len(files)} files):")
    with zipfile.ZipFile(output_wad, 'r') as wad:
        for info in sorted(wad.filelist, key=lambda x: x.filename):
            ratio = (1.0 - info.compress_size / info.file_size) * 100 if info.file_size > 0 else 0
            print(f"   {info.filename}")
            print(f"     Original: {info.file_size:,} bytes")
            print(f"     Compressed: {info.compress_size:,} bytes")
            print(f"     Ratio: {ratio:.1f}%")

def verify_wad(wad_filename):
    """Verify WAD file integrity"""
    if not os.path.exists(wad_filename):
        print(f"❌ WAD file not found: {wad_filename}")
        return False
    
    try:
        with zipfile.ZipFile(wad_filename, 'r') as wad:
            # Test integrity
            bad_file = wad.testzip()
            if bad_file:
                print(f"❌ WAD file corrupted! Bad file: {bad_file}")
                return False
            
            print(f"✅ WAD file integrity verified: {wad_filename}")
            print(f"   Files: {len(wad.filelist)}")
            return True
    except Exception as e:
        print(f"❌ Error reading WAD file: {e}")
        return False

if __name__ == "__main__":
    if len(sys.argv) > 3:
        print("Usage: python3 create_wad.py [input_dir] [output_file]")
        sys.exit(1)
    
    # Default paths
    input_dir = sys.argv[1] if len(sys.argv) > 1 else "sounds"
    output_file = sys.argv[2] if len(sys.argv) > 2 else "cometbuster.wad"
    
    create_wad(input_dir, output_file)
    
    # Verify
    print()
    verify_wad(output_file)

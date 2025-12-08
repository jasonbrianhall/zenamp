#!/usr/bin/env python3
"""
CometBuster WAD File Creator
Creates a WAD file (ZIP archive) containing game audio files (sounds and music).
WAD files compress multiple audio files into a single distributable archive.
Usage:
    python3 create_wad.py
    python3 create_wad.py <sounds_dir> <music_dir> <output_file>
    python3 create_wad.py <sounds_dir> <output_file>
"""
import zipfile
import os
import sys
from pathlib import Path

def create_wad(sounds_dir=None, music_dir=None, output_wad="cometbuster.wad"):
    """Create a WAD file (ZIP archive) from sounds and/or music directories"""
    
    # Collect all directories to process
    dirs_to_process = {}
    
    if sounds_dir and os.path.isdir(sounds_dir):
        dirs_to_process['sounds'] = sounds_dir
    elif sounds_dir:
        print(f"⚠️  Warning: Sounds directory '{sounds_dir}' not found")
    
    if music_dir and os.path.isdir(music_dir):
        dirs_to_process['music'] = music_dir
    elif music_dir:
        print(f"⚠️  Warning: Music directory '{music_dir}' not found")
    
    if not dirs_to_process:
        print(f"❌ Error: No valid directories provided or found")
        sys.exit(1)
    
    # Create ZIP file with DEFLATE compression
    print(f"Creating WAD: {output_wad}")
    print(f"Processing {len(dirs_to_process)} audio category(ies):\n")
    
    total_original = 0
    total_compressed = 0
    file_count = 0
    
    with zipfile.ZipFile(output_wad, 'w', zipfile.ZIP_DEFLATED) as wad:
        for category, dir_path in sorted(dirs_to_process.items()):
            print(f"  📁 {category.upper()}/")
            for root, dirs, files in os.walk(dir_path):
                for file in sorted(files):
                    file_path = os.path.join(root, file)
                    # Archive name includes category folder
                    rel_path = os.path.relpath(file_path, dir_path)
                    arcname = os.path.join(category, rel_path)
                    
                    file_size = os.path.getsize(file_path)
                    total_original += file_size
                    file_count += 1
                    
                    print(f"      {arcname} ({file_size:,} bytes)")
                    wad.write(file_path, arcname)
            print()
    
    # Get compressed size
    compressed_size = os.path.getsize(output_wad)
    total_compressed = compressed_size
    
    print(f"✅ WAD file created successfully!")
    print(f"   Output: {output_wad}")
    print(f"   Total files: {file_count}")
    print(f"   Original size: {total_original:,} bytes ({total_original/1024/1024:.2f} MB)")
    print(f"   Compressed size: {compressed_size:,} bytes ({compressed_size/1024/1024:.2f} MB)")
    
    if total_original > 0:
        ratio = (1.0 - compressed_size / total_original) * 100
        print(f"   Compression: {ratio:.1f}%")
    
    # List contents by category
    print(f"\n📦 WAD Contents:")
    with zipfile.ZipFile(output_wad, 'r') as wad:
        # Group files by category
        categories = {}
        for info in wad.filelist:
            parts = info.filename.split('/')
            category = parts[0] if len(parts) > 1 else 'other'
            if category not in categories:
                categories[category] = []
            categories[category].append(info)
        
        # Print organized by category
        for category in sorted(categories.keys()):
            print(f"\n   📂 {category.upper()}/")
            for info in sorted(categories[category], key=lambda x: x.filename):
                ratio = (1.0 - info.compress_size / info.file_size) * 100 if info.file_size > 0 else 0
                filename = '/'.join(info.filename.split('/')[1:])  # Remove category prefix
                print(f"      {filename}")
                print(f"        Original: {info.file_size:,} bytes | Compressed: {info.compress_size:,} bytes | Ratio: {ratio:.1f}%")

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
            
            # Count files by category
            categories = {}
            for info in wad.filelist:
                parts = info.filename.split('/')
                category = parts[0] if len(parts) > 1 else 'other'
                categories[category] = categories.get(category, 0) + 1
            
            print(f"   Total files: {len(wad.filelist)}")
            for category in sorted(categories.keys()):
                print(f"   {category.capitalize()}: {categories[category]} files")
            
            return True
    except Exception as e:
        print(f"❌ Error reading WAD file: {e}")
        return False

if __name__ == "__main__":
    if len(sys.argv) > 4:
        print("Usage:")
        print("  python3 create_wad.py                              (uses default: sounds/ & music/ dirs)")
        print("  python3 create_wad.py <output_file>                (uses default sounds/ & music/ dirs)")
        print("  python3 create_wad.py <sounds_dir> <output_file>   (sounds only)")
        print("  python3 create_wad.py <sounds_dir> <music_dir> <output_file>  (both)")
        sys.exit(1)
    
    # Parse arguments intelligently
    sounds_dir = "sounds"
    music_dir = "music"
    output_file = "cometbuster.wad"
    
    if len(sys.argv) == 2:
        # Could be output file or sounds dir
        arg = sys.argv[1]
        if arg.endswith('.wad'):
            output_file = arg
        else:
            sounds_dir = arg
    elif len(sys.argv) == 3:
        # Could be (sounds, output) or (sounds, music)
        arg1 = sys.argv[1]
        arg2 = sys.argv[2]
        if arg2.endswith('.wad'):
            sounds_dir = arg1
            output_file = arg2
            music_dir = None
        else:
            sounds_dir = arg1
            music_dir = arg2
    elif len(sys.argv) == 4:
        sounds_dir = sys.argv[1]
        music_dir = sys.argv[2]
        output_file = sys.argv[3]
    
    create_wad(sounds_dir, music_dir, output_file)
    
    # Verify
    print()
    verify_wad(output_file)

# 🎶 Zenamp

A feature-rich audio player that doesn't mess around. Built with GTK3 and SDL2, Zenamp combines modern format support with nostalgic MIDI synthesis, comprehensive karaoke capabilities, and over 30 mesmerizing visualizations. This is your complete multimedia jukebox that handles everything from vintage game music to modern karaoke nights.

## ✨ Features Overview

### 🎧 Playback Control
- **Complete Control**: Play, pause, stop, rewind/fast-forward (5-second jumps), skip tracks
- **Speed Control**: Adjust playback speed from 0.1x to 4.0x without pitch changes
- **Flexible Volume**: Scale from whisper-quiet (10%) to surprisingly loud (500%)
- **Seekable Progress**: Draggable progress bar with real-time timestamps
- **Smart Repeat**: Toggle repeat mode for endless listening

### 📜 Advanced Queue Management
- **Visual Queue Display**: See all tracks with detailed metadata (title, artist, album, genre, duration)
- **Powerful Filtering**: Real-time search/filter across all metadata fields
- **Sortable Columns**: Click any column header to sort (filename, title, artist, album, genre, time)
- **Easy File Addition**: Add files via "Add to Queue" button or Ctrl+A
- **Quick Navigation**: Jump to any track with number keys (1-9)
- **Smart Removal**: Delete tracks with keyboard shortcuts or context menu
- **CD+G Detection**: Visual indicator showing which tracks have karaoke graphics

### 🎤 Karaoke Support
Zenamp includes full karaoke capabilities with CD+G graphics support:

- **CD+G Files**: Load `.zip` files containing audio + `.cdg` graphics
- **LRC Lyrics**: Automatic conversion of `.lrc` lyric files to karaoke format
- **Two Visualization Modes**:
  - **Classic**: Traditional scrolling lyrics display
  - **Starburst**: Dynamic, audio-reactive lyric presentation
- **Real-time Sync**: Perfectly synchronized lyrics with audio playback
- **Fullscreen Mode**: Press F9 for immersive karaoke experience

### 🎨 30+ Audio Visualizations
Transform your music into visual art with real-time, audio-reactive visualizations:

**Classic Visualizations**
- **Waveform**: Traditional oscilloscope display
- **Oscilloscope**: XY mode audio plotting
- **Bars**: Frequency spectrum bars
- **Trippy Bars**: Animated, psychedelic spectrum display
- **Circle**: Circular frequency representation
- **Volume Meter**: Classic VU meter
- **Bubbles**: Floating, frequency-driven bubbles
- **Matrix Rain**: Audio-reactive Matrix-style falling characters

**Advanced Effects**
- **Fireworks**: Explosive particles triggered by audio peaks
- **DNA Helix**: Double helix that rotates with the music (2 variants)
- **Fourier Transform**: Real-time frequency domain visualization
- **Ripples**: Concentric wave patterns
- **Kaleidoscope**: Symmetric, mirror-effect patterns
- **Radial Wave**: Circular wave propagation
- **Fractal Bloom**: Trippy colorful flower like effect
- **Symmetry Cascade**: Rainbow cascading sound waves
- **Wormhole Simulation**: 3D tunnel effect

**Interactive Visualizations**
- **Sudoku Solver**: Watch a real sudoku puzzle solve itself to the beat (it also generates the puzzles)
- **Tower of Hanoi**: Classic puzzle solving in rhythm
- **Beat Chess**: AI chess game synchronized with music; fully playable as white
- **Beat Checkers**: AI checkers match moving with the beat
- **Bouncy Balls**: Physics simulation with audio-driven energy
- **Block Stack**: Stack building with the beat
- **Robot Chaser**: Animated robot following audio patterns in a maze
- **Pong**: It's table tennis; it's not super interactive but you can play against AI

**Themed Visualizations**
- **Digital Clock**: Time display with audio-reactive swirls
- **Analog Clock**: Classic clock with intensity base effect
- **Dancing Parrot**: Animated parrot character that dances to the music
- **Eye of Sauron**: All-seeing eye that reacts to music
- **Birthday Cake**: Animated cake with candles
- **Rabbit/Turtle**: Animated creature race visualization with a slow and stead turtle and rabbit that likes to take naps

**Karaoke Modes**
- **Karaoke Classic**: Traditional scrolling lyrics (for CD+G files)
- **Karaoke Starburst**: Flower in the background of the lyrics

**Pro Tips**:
- Press **F9** or **F** to toggle fullscreen visualization
- Use **Q** to cycle to next visualization
- Use **A** to cycle to previous visualization
- Adjust sensitivity slider for optimal visual response

### 🎚️ 10-Band Equalizer
Professional-grade audio equalization with precise frequency control:
- **Frequency Bands**: 31Hz, 62Hz, 125Hz, 250Hz, 500Hz, 1kHz, 2kHz, 4kHz, 8kHz, 16kHz
- **Range**: ±12dB per band
- **Real-time**: Changes apply instantly during playback
- **Presets**: Save and load your favorite EQ configurations

### 🎵 Comprehensive Format Support

**Direct Playback**
- **WAV** – Native support, zero conversion
- **AIFF** (.aif/.aiff) – Apple's audio format

**Lossless Audio**
- **FLAC** – Free Lossless Audio Codec

**Compressed Formats**
- **MP3** – Universal audio format
- **OGG Vorbis** – Open-source audio
- **Opus** – Modern, low-latency codec
- **M4A/AAC** – Advanced Audio Coding
- **WMA** – Windows Media Audio

**Karaoke Formats**
- **CD+G** – CD+Graphics karaoke files (in .zip containers)
- **LRC** – Lyric files (auto-converted to karaoke with matching audio)

**MIDI & Synthesis**
- **MIDI** (.mid/.midi) – With authentic OPL3 FM synthesis

**Generic Support**
- **Any audio format** supported by libavcodec (Linux) or Windows native codecs (.ape comes to mind but other weird formats like .au or .ra should work)

### 💾 Playlist Management
- **M3U Support**: Load and save standard M3U playlists
- **M3U8 Support**: Unicode playlist support
- **Recent Playlists**: Quick access to recently opened playlists
- **Auto-save**: Automatically remembers your last playlist and position
- **Smart Restore**: Reopens last playlist with playback position on startup

### 💫 Smart Features
- **Conversion Caching**: Converted files cached in memory for instant replay
- **Virtual File System**: Most conversions happen in RAM; CD+G/LRC conversions use temporary disk space
- **DPI Awareness**: Automatically adapts to high-DPI displays
- **Responsive Layout**: Adapts UI to screen sizes from 800x600 to 4K
- **System Tray**: Minimize to tray for background playback
- **Settings Persistence**: Remembers volume, speed, EQ, visualization preferences
- **Metadata Display**: Shows title, artist, album, genre from file tags
- **Theme Integration**: Respects system GTK theme

## 🎹 The MIDI Magic

Zenamp includes **real-time OPL3 synthesis** for MIDI files using an emulated Yamaha OPL3 chip—the same sound generator that powered classic DOS games and early PC sound cards.

Want to hear what your favorite MIDI sounded like on a 1990s Sound Blaster? Now you can. It's nerdy, it's nostalgic, and it sounds exactly like you remember (or wish you could remember).

## 🛠️ Build Requirements

### System Dependencies

#### Linux (Ubuntu/Debian)
```bash
sudo apt update
sudo apt install build-essential pkg-config
sudo apt install libgtk-3-dev libsdl2-dev libsdl2-mixer-dev
sudo apt install libvorbis-dev libogg-dev libflac-dev
sudo apt install libopusfile-dev libopus-dev
sudo apt install ffmpeg libavcodec-dev libavformat-dev libavutil-dev
```

#### Linux (Fedora/RHEL/CentOS)
```bash
sudo dnf groupinstall "Development Tools"
sudo dnf install pkg-config gtk3-devel SDL2-devel SDL2_mixer-devel
sudo dnf install libvorbis-devel libogg-devel flac-devel
sudo dnf install opusfile-devel opus-devel
sudo dnf install ffmpeg-devel
```

#### Linux (Arch)
```bash
sudo pacman -S base-devel pkg-config gtk3 sdl2 sdl2_mixer
sudo pacman -S libvorbis libogg flac opusfile opus ffmpeg
```

#### Cross-compilation for Windows (on Linux)
```bash
# Ubuntu/Debian
sudo apt install mingw-w64 mingw-w64-tools
sudo apt install mingw-w64-x86-64-dev

# You'll need mingw-w64 versions of the required libraries
# These can be built from source or installed from mingw-w64 repositories
```

### Required Libraries

| Library | Purpose | Minimum Version |
|---------|---------|-----------------|
| **GTK3** | User interface framework | 3.10+ |
| **SDL2** | Audio backend and cross-platform support | 2.0.5+ |
| **SDL2_mixer** | Additional audio mixing capabilities | 2.0.1+ |
| **libFLAC** | FLAC format decoding | 1.3.0+ |
| **libvorbis** | OGG Vorbis decoding | 1.3.0+ |
| **libogg** | OGG container support | 1.3.0+ |
| **opusfile** | Opus format decoding | 0.7+ |
| **opus** | Opus codec support | 1.1+ |
| **ffmpeg** | (Linux only) Generic audio conversion | Latest |

### Development Tools
- **GCC** or **Clang** (C++11 support required)
- **Make** (GNU Make recommended)
- **pkg-config** (for library detection)
- **MinGW-w64** (for Windows cross-compilation)

## 🔨 Building and Installing

### Quick Start (Linux)
```
git clone --recursive https://github.com/jasonbrianhall/zenamp.git
cd zenamp
git submodule update --init --recursive
# Now force-update all submodules
git submodule foreach --recursive git fetch --all
git submodule foreach --recursive git reset --hard origin/main
cd gtk3
make
./build/linux/zenamp
```

### Quick Start (Windows)

Builds are automatically created using GitHub Actions at https://github.com/jasonbrianhall/zenamp/releases

Available packages:
- **MSI** - Standard Windows installer
- **MSIX** - Microsoft Store package
- **RPM** - Fedora/RHEL/CentOS installer
- **Portable ZIP** - No installation required, run directly
- **Source code** - ZIP/tar.gz archives for building from source

### Build Options

| Command | Description |
|---------|-------------|
| `make` or `make linux` | Build for Linux (default) |
| `make windows` | Cross-compile for Windows |
| `make debug` | Build debug versions for both platforms |
| `make zenamp-linux-debug` | Build Linux version with debug symbols |
| `make zenamp-windows-debug` | Build Windows version with debug symbols |
| `make rpm` | Build RPM package (requires rpmbuild) |
| `make clean` | Remove build artifacts |
| `make clean-all` | Remove entire build directory |
| `make install` | Install to /usr/local/bin (Linux only) |
| `make help` | Show all available targets |

### Build Outputs

- **Linux**: `build/linux/zenamp`
- **Windows**: `build/windows/zenamp.exe`
- **RPM Package**: Built in rpmbuild directory structure
- **Debug versions**: Available in respective debug directories

### Windows Build Notes

The Windows build automatically:
- Includes an embedded icon if `icon.rc` is present
- Collects necessary DLL files using included `collect_dlls.sh` script
- Uses MinGW-w64 cross-compilation toolchain
- Can generate MSI and MSIX installers via GitHub Actions workflow

Required Windows DLLs will be copied to the build directory.

### RPM Package Build

To build an RPM package:
```bash
make rpm
```

Requirements:
- rpmbuild tools installed
- All build dependencies from zenamp.spec
- ImageMagick (for icon conversion)

The RPM includes:
- Desktop entry for application menu integration
- MIME type associations for all supported audio formats
- Icon installation in system icon directories

## 🚀 Usage

### Command Line
```bash
# Play a single file (clears queue)
zenamp song.mp3

# Load multiple files
zenamp *.wav *.mp3 *.mid

# Load a playlist
zenamp playlist.m3u

# Open specific karaoke file
zenamp karaoke.zip

# Load lyric file (finds matching audio automatically)
zenamp song.lrc
```

### Keyboard Shortcuts

#### Playback Control
| Key | Action |
|-----|--------|
| **Space** | Play/Pause toggle |
| **S** | Stop playback |
| **N** | Next track |
| **P** | Previous track |
| **,** or **<** or **Left Arrow** | Rewind 5 seconds |
| **.** or **>** or **Right Arrow** | Fast forward 5 seconds |
| **Home** | Jump to beginning |
| **End** | Skip to next track |
| **R** | Toggle repeat mode |
| **1-9** | Jump to queue position (1st-9th track) |

#### Volume & Visualization
| Key | Action |
|-----|--------|
| **Up Arrow** | Volume up (when not in queue) |
| **Down Arrow** | Volume down (when not in queue) |
| **Q** | Previous visualization |
| **A** | Next visualization |
| **F** or **F9** | Toggle visualization fullscreen |
| **F9** | Toggle visualization fullscreen |
| **F10** | Toggle Queue Display |
| **F11** | Toggle window fullscreen |

#### Queue Management
| Key | Action |
|-----|--------|
| **Enter** | Play selected track (when queue focused) |
| **X** or **Delete** | Remove selected/current track |
| **Up/Down Arrows** | Navigate queue (when queue focused) |

#### File Operations
| Key | Action |
|-----|--------|
| **Ctrl+O** | Open file (clears queue) |
| **Ctrl+A** | Add files to queue |
| **Ctrl+C** | Clear queue |
| **Ctrl+Q** | Quit application |

#### Help & Windows
| Key | Action |
|-----|--------|
| **F1** | Show keyboard shortcuts help |
| **F9** | Toggle visualization display |
| **F10** | Toggle Queue Display |
| **F11** | Toggle fullscreen window |
| **Esc** | Exit fullscreen (when in fullscreen) |

### Mouse Controls
- **Right-click Queue**: Context menu for remove/play actions
- **Double-click Visualization**: Toggle fullscreen
- **Click Progress Bar**: Seek to position
- **Click Queue Item**: Select (press Enter to play)
- **Click Column Headers**: Sort queue by that column

### File Menu
- **Open One File** – Opens single file, clears queue
- **Load Playlist** – Load M3U/M3U8 playlist
- **Save Playlist** – Save current queue as M3U
- **Recent Playlists** – Quick access to recent playlists
- **Add to Queue** – Add files without clearing queue
- **Clear Queue** – Remove all tracks
- **Quit** – Exit application

### Karaoke Usage
1. Load a CD+G ZIP file or LRC file
2. Visualization automatically switches to karaoke mode
3. Press F9 for fullscreen experience
4. Use A/Q to switch between Classic and Starburst modes and other visualizations
5. Playback controls work normally during karaoke

## 🎨 Visualization Features

### Sensitivity Control
Adjust how strongly visualizations respond to audio:
- Lower values (0.1-1.0): Subtle, calm effects
- Default value (1.0): Balanced response
- Higher values (2.0-5.0): Intense, dramatic effects

### Fullscreen Mode
- Press **F9** or **F** to toggle visualization fullscreen
- In fullscreen, all playback shortcuts still work
- Press **Esc** or **F9** again to exit
- Fullscreen visualization on separate screen works perfectly

### Visualization Modes Detailed

**Analysis-Based**
- **Fourier Transform**: Real mathematical frequency analysis
- **Bars/Trippy Bars**: Classic frequency spectrum display (Trippy bars has multi-colored fishies and flying animals)

**Physics Simulations**
- **Bouncy Balls**: Real physics with gravity and collisions
- **Ripples**: Wave interference patterns
- **Block Stack**: Stack building with the beat

**Game/Puzzle Solvers**
- **Sudoku**: Watches real sudoku solving algorithm (CPU-intensive, may cause brief pauses)
- **Tower of Hanoi**: Classic recursive puzzle solution
- **Beat Chess**: AI playing to music (CPU-intensive); custom engine (not a good engine but it is custom)
- **Beat Checkers**: AI checkers with beat-synced moves

**Artistic Effects**
- **Kaleidoscope**: Multi-axis symmetry
- **DNA Helix**: Rotating double helix structures
- **Matrix Rain**: Falling characters like the movie
- **Fractal Bloom**: Trippy flower

## 🔧 Troubleshooting

### Common Build Issues

**Missing pkg-config files**: 
```bash
# Check if libraries are properly installed
pkg-config --list-all | grep gtk
pkg-config --libs --cflags gtk+-3.0
```

**SDL2 not found**:
```bash
# Verify SDL2 installation
sdl2-config --version
pkg-config --libs sdl2
```

**ffmpeg not found (Linux)**:
```bash
# Install ffmpeg development packages
sudo apt install ffmpeg libavcodec-dev libavformat-dev libavutil-dev
```

**Windows cross-compilation fails**:
```bash
# Check MinGW installation
x86_64-w64-mingw32-gcc --version
mingw64-pkg-config --version
```

### Runtime Issues

**No audio output**: 
- Check SDL2 can initialize audio: `SDL_GetCurrentAudioDriver()`
- Verify volume isn't muted in system mixer
- On Linux, check PulseAudio/ALSA configuration

**MIDI files sound wrong**: 
This is expected! OPL3 synthesis recreates classic FM synthesis, not modern General MIDI. It's authentic 1990s sound.

**Missing DLLs on Windows**: 
The build process should collect required DLLs automatically. Check the `build/windows/` directory.

**Karaoke files won't load**:
- Ensure ZIP contains both audio and .cdg files
- For LRC files, matching audio file must exist in same directory
- Check console output for conversion errors

**Visualizations lag or stutter**:
- Reduce sensitivity slider
- Close other applications using GPU
- Note: Beat Chess uses ~100% CPU (multi-threaded by design)
- Note: Sudoku and Beat Checkers are CPU-intensive
- Sudoku solver may cause brief pauses (not multi-threaded)
- Minimizing the window reduces CPU usage to ~3%

**Queue filter not working**:
- Filter searches: filename, title, artist, album, genre
- Search is case-insensitive
- Clear filter with backspace/delete

### Fullscreen Visualization Tips

For the best fullscreen experience:
- Use dedicated display/monitor when possible
- Disable screen savers and power management
- Close unnecessary background applications
- Higher sensitivity values create more dramatic effects

## 💭 Design Philosophy

Zenamp does everything a local audio player should do, without compromise:

- **No Cloud**: Your music, your computer, your control
- **No Telemetry**: What you play is private
- **No Subscriptions**: Free forever, truly free
- **No Compromise**: Full features, not "lite" versions

The interface shows you what matters:
- Your queue is visible, not buried in menus
- Controls are obvious, not "intuitive" gestures
- Progress bar actually seeks, not just decorative
- Visualizations immerse, not distract

Revolutionary concepts: a music player that respects your music collection and your intelligence.

## 🎮 Technical Details

### Virtual File System
Most format conversions happen in memory using a virtual file system:
- Audio format conversions cached in RAM
- CD+G ZIP extraction uses temporary disk space
- LRC to karaoke conversion uses temporary disk space
- Instant file switching (cached conversions)
- Memory-efficient streaming for large files
- Automatic cleanup on exit

### Audio Pipeline
1. Files loaded and converted to 16-bit PCM WAV format
2. Audio passes through 10-band equalizer
3. Speed adjustment applied (time-stretching algorithm)
4. Volume scaling
5. Data fed to SDL2 audio callback and visualizer
6. Real-time frequency analysis for visualizations

### MIDI Synthesis
OPL3 emulation provides authentic FM synthesis:
- 18 operator channels
- Real-time parameter changes
- Authentic instrument patches from Sound Blaster era
- Period-correct sound characteristics
- No General MIDI—just raw FM synthesis

### Karaoke Engine
- **CD+G Parser**: Full SubCode packet interpretation
- **LRC Converter**: Automatic lyric timing extraction
- **Sync Engine**: Frame-accurate subtitle synchronization
- **Rendering**: Cairo-based high-quality text rendering

### Visualization Architecture
- Real-time FFT-like frequency analysis (simplified)
- 30+ independent visualization modules
- Audio-reactive particle systems
- Fullscreen optimization with VSync
- Scales to playback speed automatically

### Equalizer Implementation
- Butterworth filter design
- Real-time coefficient recalculation
- Per-sample processing with minimal latency
- Preset system with JSON storage

## 📊 Performance Characteristics

**Memory Usage**:
- Base: ~30-50 MB
- Per cached file: ~10-50 MB (depends on length)
- Visualization: +5-15 MB
- Typical total: 50-150 MB

**CPU Usage**:
- Idle/Stopped: <1%
- Playing (most visualizations): 10-20% depending on complexity
- Playing (Beat Chess): ~100% (multi-threaded)
- Minimized while playing: ~3%

**Supported File Sizes**:
- Tested up to 2 hour audio files
- Queue size: Unlimited (memory permitting)
- Playlist size: Unlimited

## 🎯 Known Limitations

- No streaming audio (by design—local files only)
- No video playback (audio player, not media player)
- Sudoku visualization not multi-threaded (may cause brief pauses during solving)
- Beat Chess uses ~100% CPU (multi-threaded, aggressive computation)
- Beat Checkers is CPU-intensive
- Chess/Checkers AI difficulty fixed (for consistent beat sync)
- Windows M4A/WMA support requires Windows 7+ codecs
- Two-channel support only
- My Raspberry PI choked on the queue (black screen) so I think Raspian has a bad implementation of GTK3

## 🗺️ Future Considerations

- **Visualization Plugin System**: Optional modular architecture allowing community contributions of custom visualizations without modifying core code
- **More Visualizations**: Additional visualizations will be added over time

Feature suggestions are welcomed! See the Reporting Issues section below for how to contribute ideas.

## 📄 License

MIT License – use it, modify it, distribute it. Just keep the attribution.

**Author:** Jason Hall  
**Repository:** [https://github.com/jasonbrianhall/zenamp](https://github.com/jasonbrianhall/zenamp)

## 🙏 Acknowledgments

- **OPL3 Emulation**: DosBox OPL3 emulator
- **Audio Libraries**: SDL2, libFLAC, libvorbis, opus
- **UI Framework**: GTK3
- **Chess Engine**: Custom chess Engine (not a good chess engine but it is custom)

---

*Made with C++, SDL2, and an appreciation for both modern convenience and retro authenticity. Now with karaoke!*

---

## 🐛 Reporting Issues

Found a bug? Have a feature request?

1. Check existing issues on GitHub
2. Provide system info (OS, GTK version, audio backend)
3. Include console output if available
4. Describe steps to reproduce

## 💬 Community

This is a solo project, but contributions welcome:
- Bug fixes
- New visualizations
- Format support improvements
- Documentation updates
- Translation support

---

**Star this repo if Zenamp brings joy to your music listening!** ⭐

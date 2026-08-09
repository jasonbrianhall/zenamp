#ifndef DIRECTORY_SCANNER_HPP
#define DIRECTORY_SCANNER_HPP

#include <vector>
#include <string>
#include <functional>
#include <glib.h>

// Supported music file extensions
static const std::vector<std::string> SUPPORTED_FORMATS = {
    // Lossless
    ".flac", ".alac", ".ape", ".wv", ".tta",
    // Lossy
    ".mp3", ".aac", ".ogg", ".opus", ".m4a", ".wma",
    // Uncompressed
    ".wav", ".aiff", ".aif",
    // MIDI & Module formats
    ".mid", ".midi", ".xm", ".mod", ".s3m", ".it",
    // Video with audio
    ".mp4", ".mkv", ".webm",
    // Karaoke
    ".cdg", ".kfn", ".kar", ".kok",
    // Playlist
    ".m3u", ".m3u8", ".pls", ".cue"
};

using ScanProgressCallback = std::function<void(const std::string& current_file, int total_scanned)>;

class DirectoryScanner {
public:
    DirectoryScanner() = default;
    ~DirectoryScanner() = default;

    // Scan directory for music files
    static std::vector<std::string> scanDirectory(
        const std::string& directory_path,
        bool recursive = true,
        ScanProgressCallback progress_callback = nullptr
    );

    // Check if file has a supported music extension (case-insensitive)
    static bool isSupportedMusicFile(const std::string& filepath);

    // Get file extension (lowercase)
    static std::string getFileExtension(const std::string& filepath);

private:
    // Recursive scan helper
    static void scanDirectoryRecursive(
        const std::string& path,
        std::vector<std::string>& result,
        ScanProgressCallback& callback
    );

    // Convert string to lowercase
    static std::string toLower(const std::string& str);
};

#endif // DIRECTORY_SCANNER_HPP

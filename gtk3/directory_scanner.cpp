#include "directory_scanner.hpp"
#include <algorithm>
#include <cctype>
#include <dirent.h>
#include <sys/stat.h>
#include <stdio.h>

std::string DirectoryScanner::toLower(const std::string& str) {
    std::string lower = str;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return lower;
}

std::string DirectoryScanner::getFileExtension(const std::string& filepath) {
    size_t dot_pos = filepath.find_last_of('.');
    if (dot_pos == std::string::npos) {
        return "";
    }
    return toLower(filepath.substr(dot_pos));
}

bool DirectoryScanner::isSupportedMusicFile(const std::string& filepath) {
    std::string ext = getFileExtension(filepath);
    if (ext.empty()) return false;

    return std::find(SUPPORTED_FORMATS.begin(), SUPPORTED_FORMATS.end(), ext)
        != SUPPORTED_FORMATS.end();
}

void DirectoryScanner::scanDirectoryRecursive(
    const std::string& path,
    std::vector<std::string>& result,
    ScanProgressCallback& callback
) {
    DIR* dir = opendir(path.c_str());
    if (!dir) {
        fprintf(stderr, "Failed to open directory: %s\n", path.c_str());
        return;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        // Skip . and ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        std::string full_path = path + "/" + entry->d_name;
        struct stat st;

        if (stat(full_path.c_str(), &st) == -1) {
            continue;
        }

        // If directory, recurse
        if (S_ISDIR(st.st_mode)) {
            scanDirectoryRecursive(full_path, result, callback);
        }
        // If regular file and supported format, add it
        else if (S_ISREG(st.st_mode)) {
            if (isSupportedMusicFile(full_path)) {
                result.push_back(full_path);

                if (callback) {
                    callback(full_path, result.size());
                }
            }
        }
    }

    closedir(dir);
}

std::vector<std::string> DirectoryScanner::scanDirectory(
    const std::string& directory_path,
    bool recursive,
    ScanProgressCallback progress_callback
) {
    std::vector<std::string> result;

    struct stat st;
    if (stat(directory_path.c_str(), &st) == -1) {
        fprintf(stderr, "Cannot access directory: %s\n", directory_path.c_str());
        return result;
    }

    if (!S_ISDIR(st.st_mode)) {
        fprintf(stderr, "Path is not a directory: %s\n", directory_path.c_str());
        return result;
    }

    if (recursive) {
        scanDirectoryRecursive(directory_path, result, progress_callback);
    } else {
        // Non-recursive scan
        DIR* dir = opendir(directory_path.c_str());
        if (!dir) {
            fprintf(stderr, "Failed to open directory: %s\n", directory_path.c_str());
            return result;
        }

        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }

            std::string full_path = directory_path + "/" + entry->d_name;
            struct stat entry_st;

            if (stat(full_path.c_str(), &entry_st) == -1) {
                continue;
            }

            if (S_ISREG(entry_st.st_mode) && isSupportedMusicFile(full_path)) {
                result.push_back(full_path);

                if (progress_callback) {
                    progress_callback(full_path, result.size());
                }
            }
        }

        closedir(dir);
    }

    printf("Scan complete: found %zu music files\n", result.size());
    return result;
}

/**
 * ╔══════════════════════════════════════════════════════════════════════════════╗
 * ║  file_utils.h — File System Utility Functions                             ║
 * ╚══════════════════════════════════════════════════════════════════════════════╝
 *
 * FILE PURPOSE:
 *   Common file operations used across the pipeline:
 *   - Generating unique temp file paths
 *   - Reading/writing binary files (for audio data)
 *   - URL detection
 *   - Directory management
 *
 * ALL FUNCTIONS ARE STATELESS:
 *   These are pure utility functions in a namespace — no classes, no state.
 */

#pragma once

#include <string>
#include <filesystem>
#include <vector>
#include <cstdint>

namespace vd {
namespace FileUtils {

    /// Generate a unique temp file path: base_dir + "/" + random_hex + suffix
    std::string get_temp_path(const std::string& base_dir, const std::string& suffix);

    /// Create directory tree (no-op if exists).
    void ensure_dir(const std::filesystem::path& dir);

    /// Check if a path is an HTTP/HTTPS/RTMP URL.
    bool is_url(const std::string& path);

    /// Get file extension including dot (e.g. ".mp4").
    std::string get_extension(const std::string& path);

    /// Replace file extension (e.g. "video.mp4" → "video.wav").
    std::string replace_extension(const std::string& path, const std::string& ext);

    /// Get filename without extension (e.g. "video.mp4" → "video").
    std::string get_filename_stem(const std::string& path);

    /// Recursively delete a directory and its contents.
    void cleanup_dir(const std::filesystem::path& dir);

    /// Read entire binary file into memory.  O(n) space/time.
    std::vector<uint8_t> read_file(const std::string& path);

    /// Write binary data to file.  Creates parent dirs if needed.
    void write_file(const std::string& path, const std::vector<uint8_t>& data);

} // namespace FileUtils
} // namespace vd

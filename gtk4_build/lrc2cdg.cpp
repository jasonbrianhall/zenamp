#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <memory>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <regex>
#include <filesystem>
#include <vector>
#include <string>
#include <iomanip>
#include "miniz.h" 
namespace fs = std::filesystem;

#ifdef _WIN32
#include <windows.h>
#endif

std::string get_temp_directory() {
#ifdef _WIN32
    char buffer[MAX_PATH];
    DWORD len = GetTempPathA(MAX_PATH, buffer);
    return std::string(buffer, len);
#else
    const char* tmp = getenv("TMPDIR");
    return tmp ? tmp : "/tmp";
#endif
}

// Required libraries:
// - Cairo for text rendering: libcairo2-dev
// - STB Image for image loading (header-only)
#include <cairo/cairo.h>

// For image loading, use stb_image.h (download from https://github.com/nothings/stb)
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// Constants
const int CDG_PACKETS_PER_SECOND = 300;
const int CDG_PACKET_SIZE = 24;
const int CDG_SCREEN_WIDTH = 50;
const int CDG_SCREEN_HEIGHT = 18;

// Structures
struct CDGPacket {
    uint8_t command;
    uint8_t instruction;
    uint8_t data[16];
    
    CDGPacket(uint8_t cmd, uint8_t inst, const uint8_t* d = nullptr) 
        : command(cmd), instruction(inst) {
        if (d) {
            std::memcpy(data, d, 16);
        } else {
            std::memset(data, 0, 16);
        }
    }
};

struct WordTiming {
    std::string word;
    double start;
    double end;
};

struct LyricLine {
    double timestamp;
    std::string text;
};

struct Line {
    std::vector<WordTiming> words;
    std::string text;
    double start;
    double end;
};

struct RGB4bit {
    uint8_t r, g, b;
    RGB4bit(uint8_t r_ = 0, uint8_t g_ = 0, uint8_t b_ = 0) : r(r_), g(g_), b(b_) {}
};

struct TileData {
    int row;
    std::vector<uint8_t> data;
};

struct ImageTile {
    int tile_y;
    int tile_x;
    std::vector<uint8_t> data;
};

// Helper functions
RGB4bit quantize_color_to_4bit(uint8_t r, uint8_t g, uint8_t b) {
    return RGB4bit(r >> 4, g >> 4, b >> 4);
}

CDGPacket create_memory_preset_packet(uint8_t color, uint8_t repeat = 0) {
    uint8_t data[16] = {0};
    data[0] = color & 0x0F;
    data[1] = repeat & 0x0F;
    return CDGPacket(0x09, 1, data);
}

CDGPacket create_border_preset_packet(uint8_t color) {
    uint8_t data[16] = {0};
    data[0] = color & 0x0F;
    return CDGPacket(0x09, 2, data);
}

CDGPacket create_load_color_table_low_packet(const std::vector<RGB4bit>& colors) {
    uint8_t data[16] = {0};
    for (size_t i = 0; i < 8 && i < colors.size(); i++) {
        uint8_t r = colors[i].r & 0x0F;
        uint8_t g = colors[i].g & 0x0F;
        uint8_t b = colors[i].b & 0x0F;
        uint8_t high_byte = ((r & 0x0F) << 2) | ((g & 0x0C) >> 2);
        uint8_t low_byte = ((g & 0x03) << 4) | (b & 0x0F);
        data[i * 2] = high_byte & 0x3F;
        data[i * 2 + 1] = low_byte & 0x3F;
    }
    return CDGPacket(0x09, 30, data);
}

CDGPacket create_load_color_table_high_packet(const std::vector<RGB4bit>& colors) {
    uint8_t data[16] = {0};
    for (size_t i = 0; i < 8 && i < colors.size(); i++) {
        uint8_t r = colors[i].r & 0x0F;
        uint8_t g = colors[i].g & 0x0F;
        uint8_t b = colors[i].b & 0x0F;
        uint8_t high_byte = ((r & 0x0F) << 2) | ((g & 0x0C) >> 2);
        uint8_t low_byte = ((g & 0x03) << 4) | (b & 0x0F);
        data[i * 2] = high_byte & 0x3F;
        data[i * 2 + 1] = low_byte & 0x3F;
    }
    return CDGPacket(0x09, 31, data);
}

CDGPacket create_tile_block_packet(uint8_t color0, uint8_t color1, uint8_t row, 
                                   uint8_t column, const std::vector<uint8_t>& tile_data) {
    uint8_t data[16] = {0};
    data[0] = color0 & 0x0F;
    data[1] = color1 & 0x0F;
    data[2] = row & 0x1F;
    data[3] = column & 0x3F;
    for (size_t i = 0; i < 12 && i < tile_data.size(); i++) {
        data[4 + i] = tile_data[i] & 0x3F;
    }
    return CDGPacket(0x09, 6, data);
}



// Parse LRC timestamp [mm:ss.xx] or [mm:ss.xxx]
double parse_lrc_timestamp(const std::string& timestamp) {
    std::regex time_regex(R"(\[(\d+):(\d+)\.(\d+)\])");
    std::smatch match;
    
    if (std::regex_search(timestamp, match, time_regex)) {
        int minutes = std::stoi(match[1]);
        int seconds = std::stoi(match[2]);
        std::string centiseconds_str = match[3];
        
        // Handle both .xx and .xxx formats
        double fraction = std::stod("0." + centiseconds_str);
        
        return minutes * 60.0 + seconds + fraction;
    }
    
    return 0.0;
}

// Parse LRC file
std::vector<LyricLine> parse_lrc_file(const std::string& filename) {
    std::vector<LyricLine> lines;
    std::ifstream file(filename);
    
    if (!file) {
        std::cerr << "ERROR: Could not open LRC file: " << filename << std::endl;
        return lines;
    }
    
    std::string line;
    std::regex timestamp_regex(R"(\[(\d+:\d+\.\d+)\])");
    
    while (std::getline(file, line)) {
        // Skip empty lines and metadata
        if (line.empty() || line[0] != '[') continue;
        
        // Check if it's a metadata line (e.g., [ar:Artist], [ti:Title])
        if (line.length() > 2 && line[1] >= 'a' && line[1] <= 'z' && line[2] == ':') {
            continue;
        }
        
        std::smatch match;
        if (std::regex_search(line, match, timestamp_regex)) {
            double timestamp = parse_lrc_timestamp(match[0]);
            
            // Extract text after timestamp(s)
            std::string text = std::regex_replace(line, timestamp_regex, "");
            
            // Trim whitespace
            text.erase(0, text.find_first_not_of(" \t\n\r"));
            text.erase(text.find_last_not_of(" \t\n\r") + 1);
            
            if (!text.empty()) {
                lines.push_back({timestamp, text});
            }
        }
    }
    
    // Sort by timestamp
    std::sort(lines.begin(), lines.end(), 
              [](const LyricLine& a, const LyricLine& b) { return a.timestamp < b.timestamp; });
    
    return lines;
}

// Convert LRC lines to word timings (estimate word timing within lines)
std::vector<WordTiming> lrc_to_word_timings(const std::vector<LyricLine>& lrc_lines) {
    std::vector<WordTiming> word_timings;
    
    for (size_t i = 0; i < lrc_lines.size(); i++) {
        const auto& line = lrc_lines[i];
        
        // Calculate line duration (time until next line, or 3 seconds for last line)
        double line_duration;
        if (i + 1 < lrc_lines.size()) {
            line_duration = lrc_lines[i + 1].timestamp - line.timestamp;
        } else {
            line_duration = 3.0;
        }
        
        // Split line into words
        std::istringstream iss(line.text);
        std::vector<std::string> words;
        std::string word;
        while (iss >> word) {
            words.push_back(word);
        }
        
        if (words.empty()) continue;
        
        // Distribute time evenly across words
        double time_per_word = line_duration / words.size();
        
        for (size_t j = 0; j < words.size(); j++) {
            double word_start = line.timestamp + j * time_per_word;
            double word_end = word_start + time_per_word;
            
            word_timings.push_back({words[j], word_start, word_end});
        }
    }
    
    return word_timings;
}

// Render text to tiles using Cairo
std::pair<std::vector<TileData>, int> render_text_to_tiles(const std::string& text, 
                                                            int max_width_tiles = 48, 
                                                            int font_size = 12) {
    int width_pixels = max_width_tiles * 6;
    int height_pixels = 24;
    
    cairo_surface_t* surface = cairo_image_surface_create(CAIRO_FORMAT_A8, width_pixels, height_pixels);
    cairo_t* cr = cairo_create(surface);
    
    cairo_set_source_rgb(cr, 1, 1, 1);
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, font_size);
    
    cairo_text_extents_t extents;
    cairo_text_extents(cr, text.c_str(), &extents);
    double text_width = extents.width;
    
    double x_pos = (width_pixels - text_width) / 2;
    cairo_move_to(cr, x_pos, 19);
    cairo_show_text(cr, text.c_str());
    cairo_surface_flush(surface);
    
    unsigned char* data = cairo_image_surface_get_data(surface);
    int stride = cairo_image_surface_get_stride(surface);
    
    std::vector<TileData> tiles;
    for (int tile_x = 0; tile_x < max_width_tiles; tile_x++) {
        for (int tile_row = 0; tile_row < 2; tile_row++) {
            std::vector<uint8_t> tile_data;
            for (int row = 0; row < 12; row++) {
                uint8_t bits = 0;
                for (int col = 0; col < 6; col++) {
                    int x = tile_x * 6 + col;
                    int y = tile_row * 12 + row;
                    if (x < width_pixels && y < height_pixels) {
                        uint8_t pixel = data[y * stride + x];
                        if (pixel > 128) {
                            bits |= (1 << (5 - col));
                        }
                    }
                }
                tile_data.push_back(bits);
            }
            tiles.push_back({tile_row, tile_data});
        }
    }
    
    cairo_destroy(cr);
    cairo_surface_destroy(surface);
    
    return {tiles, static_cast<int>(text_width)};
}

// Load and quantize image
std::pair<std::vector<ImageTile>, std::vector<RGB4bit>> load_and_render_image_color(const std::string& image_path) {
    int width, height, channels;
    unsigned char* img_data = stbi_load(image_path.c_str(), &width, &height, &channels, 3);
    
    if (!img_data) {
        std::cerr << "ERROR: Could not load image: " << image_path << std::endl;
        return {{}, {}};
    }
    
    std::cout << "Loaded image: " << width << "x" << height << std::endl;
    
    // Simple quantization to 16 colors
    std::vector<RGB4bit> palette;
    std::vector<std::vector<uint8_t>> pixel_map(216, std::vector<uint8_t>(300));
    
    // Generate a simple 16-color palette
    for (int i = 0; i < 16; i++) {
        palette.push_back(RGB4bit((i & 8) ? 15 : 0, (i & 4) ? 15 : 0, (i & 2) ? 15 : 0));
    }
    
    // Map pixels to palette
    for (int y = 0; y < 216; y++) {
        for (int x = 0; x < 300; x++) {
            int src_x = (x * width) / 300;
            int src_y = (y * height) / 216;
            int idx = (src_y * width + src_x) * 3;
            
            uint8_t r = img_data[idx] >> 4;
            uint8_t g = img_data[idx + 1] >> 4;
            uint8_t b = img_data[idx + 2] >> 4;
            
            // Find closest palette color
            int best_idx = 0;
            int best_dist = 999999;
            for (size_t i = 0; i < palette.size(); i++) {
                int dr = r - palette[i].r;
                int dg = g - palette[i].g;
                int db = b - palette[i].b;
                int dist = dr*dr + dg*dg + db*db;
                if (dist < best_dist) {
                    best_dist = dist;
                    best_idx = i;
                }
            }
            pixel_map[y][x] = best_idx;
        }
    }
    
    stbi_image_free(img_data);
    
    // Convert to tiles
    std::vector<ImageTile> tiles;
    for (int tile_y = 0; tile_y < CDG_SCREEN_HEIGHT; tile_y++) {
        for (int tile_x = 0; tile_x < CDG_SCREEN_WIDTH; tile_x++) {
            std::vector<uint8_t> tile_data;
            for (int row = 0; row < 12; row++) {
                uint8_t bits = 0;
                for (int col = 0; col < 6; col++) {
                    int x = tile_x * 6 + col;
                    int y = tile_y * 12 + row;
                    if (x < 300 && y < 216) {
                        uint8_t color_idx = pixel_map[y][x];
                        if (color_idx >= 8) {
                            bits |= (1 << (5 - col));
                        }
                    }
                }
                tile_data.push_back(bits);
            }
            tiles.push_back({tile_y, tile_x, tile_data});
        }
    }
    
    return {tiles, palette};
}

// Group words into lines
std::vector<Line> group_words_into_lines(const std::vector<WordTiming>& transcript,
                                         int max_chars_per_line = 40,
                                         int max_words_per_line = 6) {
    std::vector<Line> lines;
    std::vector<WordTiming> current_line_words;
    std::string current_line_text;
    
    for (const auto& entry : transcript) {
        std::string word = entry.word;
        // Trim whitespace
        word.erase(0, word.find_first_not_of(" \t\n\r"));
        word.erase(word.find_last_not_of(" \t\n\r") + 1);
        
        if (word.empty()) continue;
        
        std::string test_text = current_line_text + (current_line_text.empty() ? "" : " ") + word;
        
        if (!current_line_words.empty() && 
            (test_text.length() > static_cast<size_t>(max_chars_per_line) || 
             current_line_words.size() >= static_cast<size_t>(max_words_per_line))) {
            
            Line line;
            line.words = current_line_words;
            line.text = current_line_text;
            line.start = current_line_words[0].start;
            line.end = current_line_words.back().end;
            lines.push_back(line);
            
            current_line_words.clear();
            current_line_text = "";
            test_text = word;
        }
        
        current_line_words.push_back({word, entry.start, entry.end});
        current_line_text = test_text;
    }
    
    if (!current_line_words.empty()) {
        Line line;
        line.words = current_line_words;
        line.text = current_line_text;
        line.start = current_line_words[0].start;
        line.end = current_line_words.back().end;
        lines.push_back(line);
    }
    
    return lines;
}

// Generate CDG packets
std::vector<CDGPacket> generate_cdg_packets(const std::vector<WordTiming>& transcript,
                                            double song_duration,
                                            const std::string& image_path = "") {
    int total_packets = static_cast<int>(song_duration * CDG_PACKETS_PER_SECOND);
    
    std::cout << "Generating " << total_packets << " packets for " 
              << std::fixed << std::setprecision(2) << song_duration << " seconds" << std::endl;
    
    std::vector<CDGPacket> packets;
    
    // Load image if provided
    std::vector<ImageTile> image_tiles;
    std::vector<RGB4bit> image_palette;
    
    if (!image_path.empty()) {
        std::cout << "Loading image: " << image_path << std::endl;
        auto result = load_and_render_image_color(image_path);
        image_tiles = result.first;
        image_palette = result.second;
    }
    
    // Initialize color table
    std::vector<RGB4bit> colors_low, colors_high;
    int highlight_color;
    
    if (!image_palette.empty()) {
        colors_low.assign(image_palette.begin(), image_palette.begin() + 8);
        colors_high.assign(image_palette.begin() + 8, image_palette.end());
        colors_high[7] = RGB4bit(6, 10, 15);  // Blue highlight
        highlight_color = 15;
    } else {
        colors_low = {RGB4bit(0,0,0), RGB4bit(15,15,15), RGB4bit(15,0,0), RGB4bit(0,15,0),
                      RGB4bit(6,10,15), RGB4bit(0,0,0), RGB4bit(0,0,0), RGB4bit(0,0,0)};
        colors_high = {RGB4bit(0,0,0), RGB4bit(0,0,0), RGB4bit(0,0,0), RGB4bit(0,0,0),
                       RGB4bit(0,0,0), RGB4bit(0,0,0), RGB4bit(0,0,0), RGB4bit(0,0,0)};
        highlight_color = 4;
    }
    
    packets.push_back(create_load_color_table_low_packet(colors_low));
    packets.push_back(create_load_color_table_high_packet(colors_high));
    
    // Clear screen
    for (int repeat = 0; repeat < 16; repeat++) {
        packets.push_back(create_memory_preset_packet(0, repeat));
    }
    packets.push_back(create_border_preset_packet(0));
    
    // Fill with no-ops
    uint8_t noop_data[16] = {0};
    for (int i = packets.size(); i < total_packets; i++) {
        packets.push_back(CDGPacket(0x09, 0, noop_data));
    }
    
    // Display image
    if (!image_tiles.empty()) {
        std::cout << "Rendering image to CDG..." << std::endl;
        int start_packet = 50;
        for (size_t idx = 0; idx < image_tiles.size(); idx++) {
            int packet_idx = start_packet + idx;
            if (packet_idx < static_cast<int>(packets.size())) {
                packets[packet_idx] = create_tile_block_packet(0, 8, 
                    image_tiles[idx].tile_y, image_tiles[idx].tile_x, image_tiles[idx].data);
            }
        }
    }
    
    // Group words into lines
    auto lines = group_words_into_lines(transcript);
    std::cout << "Created " << lines.size() << " lines" << std::endl;
    
    std::vector<int> line_rows = {5, 7, 9, 11};
    std::vector<uint8_t> empty_tile(12, 0);
    
    for (size_t line_idx = 0; line_idx < lines.size(); line_idx++) {
        const auto& line = lines[line_idx];
        
        std::cout << "[" << std::setw(6) << std::setprecision(2) << line.start 
                  << "s - " << std::setw(6) << line.end << "s] Line " 
                  << std::setw(3) << (line_idx + 1) << ": " << line.text << std::endl;
        
        // Clear screen before first lyric
        if (line_idx == 0 && line.start >= 1.0) {
            double clear_start_time = std::max(0.5, line.start - 3.0);
            int clear_packet = static_cast<int>(clear_start_time * CDG_PACKETS_PER_SECOND);
            
            if (clear_packet > 50) {
                int packet_offset = 0;
                for (int row = 0; row < CDG_SCREEN_HEIGHT; row++) {
                    for (int col = 0; col < CDG_SCREEN_WIDTH; col++) {
                        int idx = clear_packet + packet_offset;
                        if (idx < static_cast<int>(packets.size())) {
                            packets[idx] = create_tile_block_packet(0, 0, row, col, empty_tile);
                        }
                        packet_offset++;
                    }
                }
            }
        }
        
        int display_position = line_idx % 4;
        
        // Render the full line
        auto [tiles, text_width_pixels] = render_text_to_tiles(line.text, 48, 12);
        int text_width_tiles = tiles.size() / 2;
        int start_column = (CDG_SCREEN_WIDTH - text_width_tiles) / 2;
        int start_packet = static_cast<int>(line.start * CDG_PACKETS_PER_SECOND);
        int base_row = line_rows[display_position];
        
        // Draw the full line
        int tile_idx = 0;
        for (int col_offset = 0; col_offset < text_width_tiles; col_offset++) {
            int col = start_column + col_offset;
            if (col >= 0 && col < CDG_SCREEN_WIDTH) {
                for (int row_offset = 0; row_offset < 2; row_offset++) {
                    if (tile_idx < static_cast<int>(tiles.size())) {
                        int packet_idx = start_packet + col_offset * 2 + row_offset;
                        if (packet_idx < static_cast<int>(packets.size())) {
                            packets[packet_idx] = create_tile_block_packet(
                                0, 1, base_row + row_offset, col, tiles[tile_idx].data);
                        }
                        tile_idx++;
                    }
                }
            }
        }
        
        // Highlight first word
        if (!line.words.empty()) {
            double word_start_time = line.words[0].start;
            int highlight_start = static_cast<int>(word_start_time * CDG_PACKETS_PER_SECOND);
            
            tile_idx = 0;
            for (int col_offset = 0; col_offset < text_width_tiles; col_offset++) {
                int col = start_column + col_offset;
                if (col >= 0 && col < CDG_SCREEN_WIDTH) {
                    for (int row_offset = 0; row_offset < 2; row_offset++) {
                        if (tile_idx < static_cast<int>(tiles.size())) {
                            int packet_idx = highlight_start + col_offset * 2 + row_offset;
                            if (packet_idx < static_cast<int>(packets.size())) {
                                packets[packet_idx] = create_tile_block_packet(
                                    0, highlight_color, base_row + row_offset, col, tiles[tile_idx].data);
                            }
                            tile_idx++;
                        }
                    }
                }
            }
            
            // Restore to white after 2 seconds
            int unhighlight_start = static_cast<int>((word_start_time + 2.0) * CDG_PACKETS_PER_SECOND);
            tile_idx = 0;
            for (int col_offset = 0; col_offset < text_width_tiles; col_offset++) {
                int col = start_column + col_offset;
                if (col >= 0 && col < CDG_SCREEN_WIDTH) {
                    for (int row_offset = 0; row_offset < 2; row_offset++) {
                        if (tile_idx < static_cast<int>(tiles.size())) {
                            int packet_idx = unhighlight_start + col_offset * 2 + row_offset;
                            if (packet_idx < static_cast<int>(packets.size())) {
                                packets[packet_idx] = create_tile_block_packet(
                                    0, 1, base_row + row_offset, col, tiles[tile_idx].data);
                            }
                            tile_idx++;
                        }
                    }
                }
            }
        }
    }
    
    return packets;
}

// Write CDG file
void write_cdg_file(const std::vector<CDGPacket>& packets, const std::string& filename) {
    std::ofstream file(filename, std::ios::binary);
    if (!file) {
        std::cerr << "ERROR: Could not open file for writing: " << filename << std::endl;
        return;
    }
    
    for (const auto& packet : packets) {
        uint8_t cmd = packet.command & 0x3F;
        uint8_t inst = packet.instruction & 0x3F;
        file.write(reinterpret_cast<const char*>(&cmd), 1);
        file.write(reinterpret_cast<const char*>(&inst), 1);
        file.write("\x00\x00", 2);
        file.write(reinterpret_cast<const char*>(packet.data), 16);
        file.write("\x00\x00\x00\x00", 4);
    }
    
    file.close();
    std::cout << "Wrote " << packets.size() << " packets to " << filename << std::endl;
}

/*int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cout << "Usage: " << argv[0] << " input.lrc output.cdg [--duration SECONDS] [--image cover.jpg]" << std::endl;
        std::cout << "  --duration: Song duration in seconds (required if not using last timestamp)" << std::endl;
        return 1;
    }
    
    std::string lrc_path = argv[1];
    std::string output_cdg = argv[2];
    std::string image_path;
    double song_duration = 0.0;
    
    // Parse arguments
    for (int i = 3; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--image" && i + 1 < argc) {
            image_path = argv[i + 1];
            i++;
        } else if (arg == "--duration" && i + 1 < argc) {
            song_duration = std::stod(argv[i + 1]);
            i++;
        }
    }
    
    std::cout << "Parsing LRC file: " << lrc_path << std::endl;
    auto lrc_lines = parse_lrc_file(lrc_path);
    
    if (lrc_lines.empty()) {
        std::cerr << "ERROR: No lyrics found in LRC file" << std::endl;
        return 1;
    }
    
    std::cout << "Found " << lrc_lines.size() << " lyric lines" << std::endl;
    
    // Calculate song duration if not provided
    if (song_duration == 0.0) {
        song_duration = lrc_lines.back().timestamp + 5.0;  // Add 5 seconds after last line
    }
    
    std::cout << "Song duration: " << std::fixed << std::setprecision(2) << song_duration << "s" << std::endl;
    
    // Convert to word timings
    std::cout << "Converting to word timings..." << std::endl;
    auto word_timings = lrc_to_word_timings(lrc_lines);
    std::cout << "Generated " << word_timings.size() << " word timings" << std::endl;
    
    std::cout << "Generating CD+G packets..." << std::endl;
    auto packets = generate_cdg_packets(word_timings, song_duration, image_path);
    
    std::cout << "Writing CDG file to " << output_cdg << "..." << std::endl;
    write_cdg_file(packets, output_cdg);
    
    int file_size = packets.size() * CDG_PACKET_SIZE;
    double expected_duration = static_cast<double>(packets.size()) / CDG_PACKETS_PER_SECOND;
    std::cout << "Done. CDG file size: " << file_size << " bytes (" << packets.size() << " packets)" << std::endl;
    std::cout << "CDG duration: " << std::fixed << std::setprecision(2) << expected_duration << " seconds" << std::endl;
    
    return 0;
}*/

bool generate_karaoke_zip_from_lrc(const std::string& lrc_path, std::string& out_zip_path) {
    fs::path lrc_file(lrc_path);
    std::string base_name = lrc_file.stem().string();
    fs::path dir = lrc_file.parent_path();

    std::vector<std::string> audio_exts = {".mp3", ".m4a", ".ogg", ".flac", ".wav", ".aif", ".aiff", ".opus", ".wma"};
    std::string audio_path;
    for (const auto& entry : fs::directory_iterator(dir)) {
        if (entry.path().stem() == base_name &&
            std::find(audio_exts.begin(), audio_exts.end(), entry.path().extension()) != audio_exts.end()) {
            audio_path = entry.path().string();
            break;
        }
    }

    if (audio_path.empty()) {
        std::cerr << "No matching audio file found for: " << lrc_path << std::endl;
        return false;
    }

    // Use system temp directory
    fs::path temp_dir = get_temp_directory();
    fs::path cdg_path = temp_dir / (base_name + ".cdg");
    fs::path zip_path = temp_dir / (base_name + ".zip");

    auto lrc_lines = parse_lrc_file(lrc_path);
    if (lrc_lines.empty()) return false;

    double duration = lrc_lines.back().timestamp + 5.0;
    auto word_timings = lrc_to_word_timings(lrc_lines);
    auto packets = generate_cdg_packets(word_timings, duration, audio_path);
    write_cdg_file(packets, cdg_path.string());

    mz_zip_archive zip;
    memset(&zip, 0, sizeof(zip));
    mz_zip_writer_init_file(&zip, zip_path.string().c_str(), 0);
    mz_zip_writer_add_file(&zip, lrc_file.filename().string().c_str(), lrc_path.c_str(), nullptr, 0, MZ_BEST_COMPRESSION);
    mz_zip_writer_add_file(&zip, cdg_path.filename().string().c_str(), cdg_path.string().c_str(), nullptr, 0, MZ_BEST_COMPRESSION);
    mz_zip_writer_add_file(&zip, fs::path(audio_path).filename().string().c_str(), audio_path.c_str(), nullptr, 0, MZ_BEST_COMPRESSION);
    mz_zip_writer_finalize_archive(&zip);
    mz_zip_writer_end(&zip);

    out_zip_path = zip_path.string();
    return true;
}

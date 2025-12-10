#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include "cometbuster.h"

// Cross-platform detection
#ifdef _WIN32
    #include <direct.h>
    #include <shlobj.h>
    #define MAX_PATH_LEN 260
    #define SEPARATOR "\\"
#else
    #include <unistd.h>
    #define MAX_PATH_LEN 512
    #define SEPARATOR "/"
#endif

// ============================================================================
// HELPER FUNCTIONS - Cross-platform path handling
// ============================================================================

static void get_config_directory(char *path, size_t max_len) {
    if (!path || max_len < 2) return;
    path[0] = '\0';
    
#ifdef _WIN32
    // Windows: Use %APPDATA%\CometBuster
    char appdata[MAX_PATH_LEN];
    appdata[0] = '\0';
    
    // Try to get APPDATA folder
    HRESULT hr = SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, appdata);
    
    if (SUCCEEDED(hr) && appdata[0] != '\0') {
        snprintf(path, max_len, "%s\\CometBuster", appdata);
        // Create directory if it doesn't exist
        _mkdir(path);
    } else {
        // Fallback to TEMP directory
        char temp[MAX_PATH_LEN];
        GetTempPathA(MAX_PATH_LEN, temp);
        // Remove trailing backslash if present
        size_t len = strlen(temp);
        if (len > 0 && temp[len-1] == '\\') {
            temp[len-1] = '\0';
        }
        snprintf(path, max_len, "%s\\CometBuster", temp);
        _mkdir(path);
    }
#else
    // Linux/macOS: Use ~/.cometbuster (for compatibility with existing comet_main.cpp)
    const char *home = getenv("HOME");
    if (!home || home[0] == '\0') {
        home = "/tmp";
    }
    
    snprintf(path, max_len, "%s/.cometbuster", home);
    // Create directory if it doesn't exist
    mkdir(path, 0755);
#endif
}

// ============================================================================
// HIGH SCORE MANAGEMENT
// ============================================================================

void comet_buster_load_high_scores(CometBusterGame *game) {
    if (!game) return;
    
    // Initialize high score array
    game->high_score_count = 0;
    memset(game->high_scores, 0, sizeof(game->high_scores));
    
    // Get config directory
    char config_dir[MAX_PATH_LEN];
    get_config_directory(config_dir, sizeof(config_dir));
    
    if (config_dir[0] == '\0') {
        // Failed to get config directory
        return;
    }
    
    char filepath[MAX_PATH_LEN * 2];
    snprintf(filepath, sizeof(filepath), "%s%shighscores.txt", config_dir, SEPARATOR);
    
    FILE *file = fopen(filepath, "r");
    if (!file) {
        // File doesn't exist yet - that's fine, we'll create it on save
        fprintf(stderr, "[HIGH SCORES] No existing high scores file at: %s\n", filepath);
        return;
    }
    
    // Read high scores from file (text format: score wave timestamp name)
    int count = 0;
    while (count < MAX_HIGH_SCORES) {
        int score, wave;
        time_t timestamp;
        char name[32];
        
        if (fscanf(file, "%d %d %ld %31s\n", &score, &wave, &timestamp, name) != 4) {
            break;
        }
        
        HighScore *hs = &game->high_scores[count];
        hs->score = score;
        hs->wave = wave;
        hs->timestamp = timestamp;
        strncpy(hs->player_name, name, sizeof(hs->player_name) - 1);
        hs->player_name[sizeof(hs->player_name) - 1] = '\0';
        
        count++;
    }
    
    fclose(file);
    game->high_score_count = count;
    fprintf(stderr, "[HIGH SCORES] Loaded %d high scores from: %s\n", count, filepath);
}

void comet_buster_save_high_scores(CometBusterGame *game) {
    if (!game) return;
    
    // Get config directory
    char config_dir[MAX_PATH_LEN];
    get_config_directory(config_dir, sizeof(config_dir));
    
    if (config_dir[0] == '\0') {
        // Failed to get config directory
        fprintf(stderr, "Failed to get config directory for saving high scores\n");
        return;
    }
    
    char filepath[MAX_PATH_LEN * 2];
    snprintf(filepath, sizeof(filepath), "%s%shighscores.txt", config_dir, SEPARATOR);
    
    FILE *file = fopen(filepath, "w");
    if (!file) {
        fprintf(stderr, "Failed to open high scores file for writing: %s\n", filepath);
        return;
    }
    
    // Write all high scores to file (text format: score wave timestamp name)
    for (int i = 0; i < game->high_score_count && i < MAX_HIGH_SCORES; i++) {
        HighScore *hs = &game->high_scores[i];
        fprintf(file, "%d %d %ld %s\n", hs->score, hs->wave, hs->timestamp, hs->player_name);
    }
    
    fclose(file);
    fprintf(stderr, "[HIGH SCORES] Saved %d high scores to: %s\n", game->high_score_count, filepath);
}

bool comet_buster_is_high_score(CometBusterGame *game, int score) {
    if (!game) return false;
    
    // If we have fewer than max scores, it's always a high score
    if (game->high_score_count < MAX_HIGH_SCORES) {
        return true;
    }
    
    // Otherwise, check if score beats the lowest high score
    // High scores are sorted in descending order (highest first)
    if (game->high_score_count > 0) {
        return score > game->high_scores[game->high_score_count - 1].score;
    }
    
    return false;
}

void comet_buster_add_high_score(CometBusterGame *game, int score, int wave, const char *name) {
    if (!game || !name) return;
    
    // Find insertion point (scores should be in descending order)
    int insert_pos = game->high_score_count;
    
    for (int i = 0; i < game->high_score_count; i++) {
        if (score > game->high_scores[i].score) {
            insert_pos = i;
            break;
        }
    }
    
    // Don't add if it doesn't make the top 10
    if (insert_pos >= MAX_HIGH_SCORES) {
        return;
    }
    
    // Shift scores down to make room
    if (game->high_score_count < MAX_HIGH_SCORES) {
        // We have room, just shift from insert_pos down
        for (int i = game->high_score_count; i > insert_pos; i--) {
            game->high_scores[i] = game->high_scores[i - 1];
        }
        game->high_score_count++;
    } else {
        // We're at max, shift everything after insert_pos down (losing the lowest)
        for (int i = MAX_HIGH_SCORES - 1; i > insert_pos; i--) {
            game->high_scores[i] = game->high_scores[i - 1];
        }
    }
    
    // Insert the new high score
    game->high_scores[insert_pos].score = score;
    game->high_scores[insert_pos].wave = wave;
    game->high_scores[insert_pos].timestamp = time(NULL);
    strncpy(game->high_scores[insert_pos].player_name, name, 
            sizeof(game->high_scores[insert_pos].player_name) - 1);
    game->high_scores[insert_pos].player_name[sizeof(game->high_scores[insert_pos].player_name) - 1] = '\0';
    
    // Save to disk
    comet_buster_save_high_scores(game);
}

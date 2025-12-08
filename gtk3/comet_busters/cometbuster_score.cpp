#include <cairo.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "cometbuster.h"
#include "visualization.h"

#ifdef ExternalSound
#include "audio_wad.h"
#endif 

void comet_buster_load_high_scores(CometBusterGame *game) {
    if (!game) return;
    
    game->high_score_count = 0;
    // Initialize high scores array
    for (int i = 0; i < MAX_HIGH_SCORES; i++) {
        game->high_scores[i].score = 0;
        game->high_scores[i].wave = 0;
        game->high_scores[i].timestamp = 0;
        game->high_scores[i].player_name[0] = '\0';
    }
    // Note: Actual loading is done by comet_main.cpp (high_scores_load function)
    // This function is kept here for API compatibility
}


void comet_buster_save_high_scores(CometBusterGame *game) {
    if (!game) return;
    // Note: Actual saving is done by comet_main.cpp (high_scores_save function)
    // This function is kept here for API compatibility
}

void comet_buster_add_high_score(CometBusterGame *game, int score, int wave, const char *name) {
    return;
    if (!game || !name) return;
    // Find insertion position to maintain sorted order (highest score first)
    int insert_pos = game->high_score_count;
    for (int i = 0; i < game->high_score_count; i++) {
        if (score > game->high_scores[i].score) {
            insert_pos = i;
            break;
        }
    }
    
    // Don't add if score is below the 10th place and we're already full
    if (insert_pos >= MAX_HIGH_SCORES) {
        return;
    }
    
    // Shift scores down to make room, removing the lowest score if list is full
    if (game->high_score_count >= MAX_HIGH_SCORES) {
        // List is full, shift entries down from the insert position
        for (int i = MAX_HIGH_SCORES - 1; i > insert_pos; i--) {
            game->high_scores[i] = game->high_scores[i - 1];
        }
    } else {
        // List has room, shift entries down and increment count
        for (int i = game->high_score_count; i > insert_pos; i--) {
            game->high_scores[i] = game->high_scores[i - 1];
        }
        game->high_score_count++;
    }
    
    // Insert the new high score at the correct position
    HighScore *hs = &game->high_scores[insert_pos];
    hs->score = score;
    hs->wave = wave;
    hs->timestamp = time(NULL);
    strncpy(hs->player_name, name, sizeof(hs->player_name) - 1);
    hs->player_name[sizeof(hs->player_name) - 1] = '\0';
    
    fprintf(stdout, "[HIGH SCORE] Added new high score: %d points at position %d\n", score, insert_pos + 1);
}

bool comet_buster_is_high_score(CometBusterGame *game, int score) {
    if (!game) return false;
    
    // If we have fewer than 10 scores, any score is a high score
    if (game->high_score_count < MAX_HIGH_SCORES) return true;
    
    // If we have 10 scores, check if the score beats the lowest (10th) score
    if (game->high_score_count >= MAX_HIGH_SCORES) {
        return score > game->high_scores[MAX_HIGH_SCORES - 1].score;
    }
    
    return false;
}


// metadata.c
#include <taglib/tag_c.h>
#include <stdio.h>
#include <string.h>
#include <glib.h>

char* extract_metadata(const char *filepath) {
    TagLib_File *file = taglib_file_new(filepath);
    if (!file || !taglib_file_is_valid(file)) {
        if (file) taglib_file_free(file);
        return g_strdup("No metadata available");
    }
    
    TagLib_Tag *tag = taglib_file_tag(file);
    const TagLib_AudioProperties *props = taglib_file_audioproperties(file);
    
    char metadata[1024] = "";
    
    if (tag) {
        const char *title = taglib_tag_title(tag);
        const char *artist = taglib_tag_artist(tag);
        const char *album = taglib_tag_album(tag);
        const char *genre = taglib_tag_genre(tag);  // ADD THIS
        unsigned int year = taglib_tag_year(tag);
        
        if (title && strlen(title) > 0)
            snprintf(metadata + strlen(metadata), sizeof(metadata) - strlen(metadata), 
                    "<b>Title:</b> %s\n", title);
        if (artist && strlen(artist) > 0)
            snprintf(metadata + strlen(metadata), sizeof(metadata) - strlen(metadata), 
                    "<b>Artist:</b> %s\n", artist);
        if (album && strlen(album) > 0)
            snprintf(metadata + strlen(metadata), sizeof(metadata) - strlen(metadata), 
                    "<b>Album:</b> %s\n", album);
        if (genre && strlen(genre) > 0)  // ADD THIS
            snprintf(metadata + strlen(metadata), sizeof(metadata) - strlen(metadata), 
                    "<b>Genre:</b> %s\n", genre);
        if (year > 0)
            snprintf(metadata + strlen(metadata), sizeof(metadata) - strlen(metadata), 
                    "<b>Year:</b> %u\n", year);
    }
    
    if (props) {
        int duration = taglib_audioproperties_length(props);
        int bitrate = taglib_audioproperties_bitrate(props);
        int samplerate = taglib_audioproperties_samplerate(props);
        int channels = taglib_audioproperties_channels(props);
        
        if (duration > 0) {
            int minutes = duration / 60;
            int seconds = duration % 60;
            snprintf(metadata + strlen(metadata), sizeof(metadata) - strlen(metadata), 
                    "<b>Duration:</b> %d:%02d\n", minutes, seconds);
        }
        
        if (bitrate > 0)
            snprintf(metadata + strlen(metadata), sizeof(metadata) - strlen(metadata), 
                    "<b>Bitrate:</b> %d kbps\n", bitrate);
        if (samplerate > 0)
            snprintf(metadata + strlen(metadata), sizeof(metadata) - strlen(metadata), 
                    "<b>Sample Rate:</b> %d Hz\n", samplerate);
        if (channels > 0)
            snprintf(metadata + strlen(metadata), sizeof(metadata) - strlen(metadata), 
                    "<b>Channels:</b> %d\n", channels);
    }
    
    taglib_file_free(file);
    
    if (strlen(metadata) == 0) {
        return g_strdup("No metadata available");
    }
    
    return g_strdup(metadata);
}

int get_file_duration(const char *filepath) {
    TagLib_File *file = taglib_file_new(filepath);
    if (!file || !taglib_file_is_valid(file)) {
        if (file) taglib_file_free(file);
        return 0;
    }
    
    const TagLib_AudioProperties *props = taglib_file_audioproperties(file);
    int duration = 0;
    
    if (props) {
        duration = taglib_audioproperties_length(props);
    }
    
    taglib_file_free(file);
    return duration;
}

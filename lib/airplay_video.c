/**
 * Copyright (c) 2024 fduncanh
 * All Rights Reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 */


//airplay_video service should handle interactions with the media player, such as pause, stop, start, scrub  etc.
// it should only start and stop the media_data_store that handles all HLS transactions, without
// otherwise participating in them

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
//#include <errno.h>
#include <assert.h>
#include <plist/plist.h>

#include "raop.h"
//#include "compat.h"
#include "utils.h"
#include "airplay_video.h"


struct airplay_video_s {
    raop_t *raop;
    logger_t *logger;
    raop_callbacks_t callbacks;
    void *conn_opaque;
    char apple_session_id[37];
    char playback_uuid[37];
    float start_position_seconds;
    playback_info_t *playback_info;

    // The local port of the airplay server on the AirPlay server
    unsigned short airplay_port;

    // the TCP socket used for reverse  HTTP
    int rsock;
  
};



int set_playback_info_item(airplay_video_t *airplay_video, const char *item, int num, float *val) {
    playback_info_t *playback_info = airplay_video->playback_info;
    return 0;
    if (strstr(item, "duration")) {
        playback_info->duration = *val;
    } else if (strstr(item, "position")) {
        playback_info->position = *val;
    } else if (strstr(item, "rate")) {
        playback_info->rate = *val;
    } else if (strstr(item, "readyToPlay")) {
      playback_info->ready_to_play  = !!num;
    } else if (strstr(item, "playbackBufferEmpty")) {
        playback_info->playback_buffer_empty  = !!num;
    } else if (strstr(item, "playbackBufferFull")) {
        playback_info->playback_buffer_full  = !!num;
    } else if (strstr(item, "playbackLikelyToKeepUp")) {
        playback_info->playback_likely_to_keep_up  = !!num;
    } else if (strstr(item, "loadedTimeRanges")) {
        if (num < 0 || num > MAX_TIME_RANGES) {
	    return -1;
        }
        playback_info->num_loaded_time_ranges = num;
    } else if (strstr(item, "seekableTimeRanges")) {
        if (num < 0 || num > MAX_TIME_RANGES) {
	    return -1;
        }
        playback_info->num_seekable_time_ranges  = num;
    } else {
        return -1;    
    }
    return 0;
}



//  initialize airplay_video service.

airplay_video_t *airplay_video_service_init(logger_t *logger, raop_callbacks_t *callbacks, void *conn_opaque,
                                            raop_t *raop, unsigned short http_port, const char *session_id) {

    void *media_data_store = NULL;
    assert(logger);
    assert(callbacks);
    assert(conn_opaque);
    assert(raop);
    airplay_video_t *airplay_video =  (airplay_video_t *) calloc(1, sizeof(airplay_video_t));
    if (!airplay_video) {
        return NULL;
    }
    
    /* destroy any existing media_data_store and create a new instance*/
    set_media_data_store(raop, media_data_store);  
    media_data_store = media_data_store_create(conn_opaque, http_port);
    logger_log(logger, LOGGER_DEBUG, "airplay_video_service_init: media_data_store created at %p", media_data_store);
    set_media_data_store(raop, media_data_store);

    //    char *plist_xml;
    //airplay_video_acquire_playback_info(airplay_video, "session_id", &plist_xml);   
    //printf("%s\n", plist_xml);
    //free (plist_xml);
      
    airplay_video->logger = logger;
    memcpy(&airplay_video->callbacks, callbacks, sizeof(raop_callbacks_t));
    airplay_video->raop = raop;
    airplay_video->conn_opaque = conn_opaque;

    size_t len = strlen(session_id);
    assert(len == 36);
    strncpy(airplay_video->apple_session_id, session_id, len);
    (airplay_video->apple_session_id)[len] = '\0';

    airplay_video->start_position_seconds = 0.0f;
    


    return airplay_video;
}



// destroy the airplay_video service
void
airplay_video_service_destroy(airplay_video_t *airplay_video)
{

    void* media_data_store = NULL;
    /* destroys media_data_store if called with media_data_store = NULL */
    set_media_data_store(airplay_video->raop, media_data_store);
    
}

const char *get_apple_session_id(airplay_video_t *airplay_video) {
    return airplay_video->apple_session_id;
}

float get_start_position_seconds(airplay_video_t *airplay_video) {
    return airplay_video->start_position_seconds;
}

void set_start_position_seconds(airplay_video_t *airplay_video, float start_position_seconds) {
    airplay_video->start_position_seconds = start_position_seconds;
}
  
void set_playback_uuid(airplay_video_t *airplay_video, const char *playback_uuid) {
    size_t len = strlen(playback_uuid);
    assert(len == 36);
    memcpy(airplay_video->playback_uuid, playback_uuid, len);
    (airplay_video->playback_uuid)[len] = '\0';
}

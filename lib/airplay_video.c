/*
 * Copyright (c) 2019 dsafa22 and 2014 Joakim Plate, modified by Florian Draschbacher,
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
 *=================================================================
 * modified by fduncanh 2021-23
 */

// Some of the code in here comes from https://github.com/juhovh/shairplay/pull/25/files

//airplay_video service should handle interactions with the media player, such as pause, stop, start, scrub  etc.
// it should only start and stop the media_data_store that handles all HLS transactions, without
// otherwise participating in them

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <assert.h>
#include <plist/plist.h>
//#include <gst/gst.h>

#include "raop.h"
#include "threads.h"
#include "compat.h"
#include "utils.h"
#include "airplay_video.h"

#define MAX_TIME_RANGES 10

typedef struct time_range_s {
    double start;
    double duration;
} time_range_t;

typedef struct playback_info_s {
    // char * uuid
    // uint32_t stallCount
    double duration;
    double position;
    double rate;
    bool readyToPlay;
    bool playbackBufferEmpty;
    bool playbackBufferFull;
    bool playbackLikelyToKeepUp;
    int num_loaded_time_ranges;
    int num_seekable_time_ranges;
    time_range_t loadedTimeRanges[MAX_TIME_RANGES];
    time_range_t seekableTimeRanges[MAX_TIME_RANGES];
} playback_info_t;

struct airplay_video_s {
    raop_t *raop;
    logger_t *logger;
    raop_callbacks_t callbacks;
    void *conn_opaque;
    char *session_id;
    playback_info_t *playback_info;
  
    thread_handle_t thread;
    mutex_handle_t run_mutex;

    mutex_handle_t wait_mutex;
    cond_handle_t wait_cond;

    // The local port of the airplay server on the AirPlay server
    unsigned short airplay_port;

    // the TCP socket used for reverse  HTTP
    int rsock;
  
    /* MUTEX LOCKED VARIABLES START */
    /* These variables only edited mutex locked */
    int running;
    int joined;
};


static playback_info_t playback_info_static;

static time_range_t loaded_time_ranges[MAX_TIME_RANGES];
static time_range_t seekable_time_ranges[MAX_TIME_RANGES];

int set_playback_info_item(playback_info_t *playback_info, const char *item, double val) {
     if (strstr(item, "duration")) {
        playback_info->duration = val;
    } else if (strstr(item, "position")) {
        playback_info->position = val;
    } else if (strstr(item, "rate")) {
        playback_info->rate = val;
    } else if (strstr(item, "readyToPlay")) {
        playback_info->readyToPlay  = (val ? true : false);
    } else if (strstr(item, "playbackBufferEmpty")) {
        playback_info->playbackBufferEmpty  = (val ? true : false);
    } else if (strstr(item, "playbackBufferFull")) {
        playback_info->playbackBufferFull  = (val ? true : false);
    } else if (strstr(item, "playbackLikelyToKeepUp")) {
        playback_info->playbackLikelyToKeepUp  = (val ? true : false);
    } else if (strstr(item, "loadedTimeRanges")) {
        int num = (int) val;
        if (num < 0 || num > MAX_TIME_RANGES) {
	    return -1;
        }
        playback_info->num_loaded_time_ranges = num;
    } else if (strstr(item, "seekableTimeRanges")) {
        int num = (int) val;
        if (num < 0 || num > MAX_TIME_RANGES) {
	    return -1;
        }
        playback_info->num_seekable_time_ranges  = (int) val;
    } else {
        return -1;    
    }
    return 0;
}

/* to add a new time_range to a playback_info_t struct::
 *    e.g.:  add_playback_info_time_range(playback_info, "loadedTimeRange", 65.5,  12.3);
 *    where duration = 65.5 secs, start = 12.3 secs: returns 0 if success, -1 if fail.
 *    This adds a  new loadedTimeRange at  playback_info->loadedTimeRange[num]
 *    where num is playback_info->num_loaded_time_ranges prior to the call.
 *    attempts to add another time_range if num == MAX_TIME_RANGES will fail
 *    
 *    Replace "loaded" by "seekable" for handling a seekableTimeRange.
 */


// this adds a time range (duration, start) of time-range_type = "loadedTimeRange" or "seekableTimeRange"
// to the playback info struc, and increments the appropiate counter by 1.  Not more than
// MAX_TIME_RANGES of a give typen may be added.
// returns 0 for success, -1 for failure.

int add_playback_info_time_range(playback_info_t *playback_info, const char *time_range_type, double duration, double start) {
    time_range_t *time_range;
    int *time_range_num;
    if (!strstr(time_range_type, "loadedTimeRange")) {
        time_range_num = &playback_info->num_loaded_time_ranges;
        time_range = (time_range_t *) &playback_info->loadedTimeRanges[*time_range_num];
    } else if (!strstr(time_range_type, "seekableTimeRange")) {
        time_range_num = &playback_info->num_seekable_time_ranges;
        time_range = (time_range_t *) &playback_info->seekableTimeRanges[*time_range_num];
    } else {
        return -1;
    }
    
    if (*time_range_num == MAX_TIME_RANGES) {
        return -1;
    }
    time_range->duration = duration;
    time_range->start = start;
    (*time_range_num)++;
    return 0;
}


// this allows the entries in playback_info to be updated.

static
void initialize_playback_info(airplay_video_t *airplay_video) {
    int ret = 0;  
    playback_info_t * playback_info = (playback_info_t *) &(airplay_video->playback_info);
    ret += set_playback_info_item(playback_info, "duration", 0);
    ret += set_playback_info_item(playback_info, "position", 0);
    ret += set_playback_info_item(playback_info, "rate", 0);
    ret += set_playback_info_item(playback_info, "readyToPlay", 0);
    ret += set_playback_info_item(playback_info, "playbackBufferEmpty",0);
    ret += set_playback_info_item(playback_info, "playbackBufferFull", 0);
    ret += set_playback_info_item(playback_info, "playbackLikelyToKeepUp", 1);
    ret += set_playback_info_item(playback_info, "loadedTimeRanges", 0);
    ret += set_playback_info_item(playback_info, "seekableTimeRanges", 0);

    // for testing time_range code
    //ret += add_playback_info_time_range(playback_info, "seekableTimeRange", 0.3,  0.3);

    if (ret) {
          logger_log(airplay_video->logger, LOGGER_ERR, "initialize_playback_info error");
    }
}


// called by http_handler_playback_info to respond to a GET /playback_info request from the client.

int airplay_video_acquire_playback_info(airplay_video_t *airplay_video, const char *session_id, char **plist_xml) {

    playback_info_t *playback_info  = (playback_info_t*) &airplay_video->playback_info;
    printf ("playback_info %p\n", playback_info);
    plist_t res_root_node = plist_new_dict();

    plist_t duration_node = plist_new_real(playback_info->duration);
    plist_dict_set_item(res_root_node, "duration", duration_node);

    plist_t position_node = plist_new_real(playback_info->position);
    plist_dict_set_item(res_root_node, "position", position_node);

    plist_t rate_node = plist_new_real(playback_info->rate);
    plist_dict_set_item(res_root_node, "rate", rate_node);

    plist_t ready_to_play_node = plist_new_bool(playback_info->readyToPlay);
    plist_dict_set_item(res_root_node, "readyToPlay", ready_to_play_node);

    plist_t playback_buffer_empty_node = plist_new_bool(playback_info->playbackBufferEmpty);
    plist_dict_set_item(res_root_node, "playbackBufferEmpty", playback_buffer_empty_node);

    plist_t playback_buffer_full_node = plist_new_bool(playback_info->playbackBufferFull);
    plist_dict_set_item(res_root_node, "playbackBufferFull", playback_buffer_full_node);

    plist_t playback_likely_to_keep_up_node = plist_new_bool(playback_info->playbackLikelyToKeepUp);
    plist_dict_set_item(res_root_node, "playbackLikelyToKeepUp", playback_likely_to_keep_up_node);

    plist_t loaded_time_ranges_node = plist_new_array();

    printf("num_loaded_time_ranges %d\n",playback_info->num_loaded_time_ranges);
    
    for (int i = 0 ; i < playback_info->num_loaded_time_ranges; i++) {
        assert (i < MAX_TIME_RANGES);
	time_range_t *time_range = &playback_info->loadedTimeRanges[i];
        plist_t time_range_node = plist_new_dict();
        plist_t duration_node = plist_new_real( time_range->duration);
        plist_dict_set_item(time_range_node, "duration", duration_node);
        plist_t start_node = plist_new_real( time_range->start);
        plist_dict_set_item(time_range_node, "start", start_node);
        plist_array_append_item(loaded_time_ranges_node, time_range_node);
    }
    plist_dict_set_item(res_root_node, "loadedTimeRanges", loaded_time_ranges_node);

    plist_t seekable_time_ranges_node = plist_new_array();
    for (int i = 0 ; i < playback_info->num_seekable_time_ranges; i++) {
        assert (i < MAX_TIME_RANGES);
	time_range_t *time_range = &playback_info->loadedTimeRanges[i];
        plist_t time_range_node = plist_new_dict();
        plist_t duration_node = plist_new_real(time_range->duration);
        plist_dict_set_item(time_range_node, "duration", duration_node);
        plist_t start_node = plist_new_real(time_range->start);
        plist_dict_set_item(time_range_node, "start", start_node);
        plist_array_append_item(seekable_time_ranges_node, time_range_node);
    }
    plist_dict_set_item(res_root_node, "seekableTimeRanges", seekable_time_ranges_node);

    uint32_t len;
    plist_to_xml(res_root_node, plist_xml, &len);
    plist_free(res_root_node);
    return (int) len;
}


// a thread for the airplay_video server, (may not be necessary, but included in case it is)

static THREAD_RETVAL
airplay_video_thread(void *arg)
{
    airplay_video_t *airplay_video = arg;
    assert(airplay_video);

    //bool logger_debug = (logger_get_level(airplay_video->logger) >= LOGGER_DEBUG);
      
    while (1) {
        MUTEX_LOCK(airplay_video->run_mutex);
        if (!airplay_video->running) {
            MUTEX_UNLOCK(airplay_video->run_mutex);
            break;
        }
        MUTEX_UNLOCK(airplay_video->run_mutex);

        logger_log(airplay_video->logger, LOGGER_INFO, "airplay_video_service thread is running");

        // Sleep for 3 seconds
        struct timespec wait_time;
        MUTEX_LOCK(airplay_video->wait_mutex);
        clock_gettime(CLOCK_REALTIME, &wait_time);
        wait_time.tv_sec += 3;
        pthread_cond_timedwait(&airplay_video->wait_cond, &airplay_video->wait_mutex, &wait_time);
        MUTEX_UNLOCK(airplay_video->wait_mutex);
    }

    // Ensure running reflects the actual state
    MUTEX_LOCK(airplay_video->run_mutex);
    airplay_video->running = false;
    MUTEX_UNLOCK(airplay_video->run_mutex);

    logger_log(airplay_video->logger, LOGGER_DEBUG, "airplay_video exiting thread");
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
    set_session_id(media_data_store, session_id);


    initialize_playback_info(airplay_video);
    //    char *plist_xml;
    //airplay_video_acquire_playback_info(airplay_video, "session_id", &plist_xml);   
    //printf("%s\n", plist_xml);
    //free (plist_xml);
      
    airplay_video->logger = logger;
    memcpy(&airplay_video->callbacks, callbacks, sizeof(raop_callbacks_t));
    airplay_video->raop = raop;
    airplay_video->conn_opaque = conn_opaque;

    size_t len = strlen(session_id);
    airplay_video->session_id = (char *) malloc(len + 1);
    strncpy(airplay_video->session_id, session_id, len);
    (airplay_video->session_id)[len] = '\0';

    airplay_video->running = 0;
    airplay_video->joined = 1;


    MUTEX_CREATE(airplay_video->run_mutex);
    MUTEX_CREATE(airplay_video->wait_mutex);
    COND_CREATE(airplay_video->wait_cond);
    return airplay_video;
}


// start the already-initialized airplay_video service by  creating a thread
void
airplay_video_service_start(airplay_video_t *airplay_video)
{
    logger_log(airplay_video->logger, LOGGER_DEBUG, "airplay_video_service is starting");


    MUTEX_LOCK(airplay_video->run_mutex);
    if (airplay_video->running || !airplay_video->joined) {
        MUTEX_UNLOCK(airplay_video->run_mutex);
        return;
    }
    
    /* Create the thread and initialize running values */
    airplay_video->running = 1;
    airplay_video->joined = 0;
    
    THREAD_CREATE(airplay_video->thread, airplay_video_thread, airplay_video);
    MUTEX_UNLOCK(airplay_video->run_mutex);
}

//stop the airplay_video thread
void
airplay_video_service_stop(airplay_video_t *airplay_video)
{
    assert(airplay_video);

    /* Check that we are running and thread is not
     * joined (should never be while still running) */
    MUTEX_LOCK(airplay_video->run_mutex);
    if (!airplay_video->running || airplay_video->joined) {
        MUTEX_UNLOCK(airplay_video->run_mutex);
        return;
    }
    airplay_video->running = 0;
    MUTEX_UNLOCK(airplay_video->run_mutex);

    logger_log(airplay_video->logger, LOGGER_DEBUG, "airplay_video stopping airplay_video_service thread");

    MUTEX_LOCK(airplay_video->wait_mutex);
    COND_SIGNAL(airplay_video->wait_cond);
    MUTEX_UNLOCK(airplay_video->wait_mutex);
    /*
    // should the TCP socket for airplay video be closed
    if (raop_ntp->tsock != -1) {
        closesocket(raop_ntp->tsock);
        raop_ntp->tsock = -1;
    }
    */
    THREAD_JOIN(airplay_video->thread);

    logger_log(airplay_video->logger, LOGGER_DEBUG, "airplay_video stopped airplay_video_service thread");

    /* Mark thread as joined */
    MUTEX_LOCK(airplay_video->run_mutex);
    airplay_video->joined = 1;
    MUTEX_UNLOCK(airplay_video->run_mutex);
}

// destroy the airplay_video service
void
airplay_video_service_destroy(airplay_video_t *airplay_video)
{
    airplay_video_service_stop(airplay_video);
    MUTEX_DESTROY(airplay_video->run_mutex);
    MUTEX_DESTROY(airplay_video->wait_mutex);
    COND_DESTROY(airplay_video->wait_cond);


    void* media_data_store = NULL;
    /* destroys media_data_store if called with media_data_store = NULL */
    set_media_data_store(airplay_video->raop, media_data_store);
    
    if (airplay_video->session_id) {
        free (airplay_video->session_id);
    }
    free(airplay_video);
}



/* (partially implemented) call to handle the "POST /play" request from client.*/

void airplay_video_play(airplay_video_t *airplay_video, const char *session_id, const char *location, double start_position) {

    printf("airplay_video_play %s, start_position %f\n", location, start_position);

    char play_cmd[] = "nohup gst-launch-1.0 playbin uri=";
    size_t len = strlen(play_cmd) + strlen(location) + 3;
    char *command = (char *) calloc(len, sizeof(char));
    strncat(command, play_cmd, len);
    strncat(command, location, len);
    strncat(command, " &", len);
    logger_log(airplay_video->logger, LOGGER_INFO, "HLS player command is: \"%s\"", command);
  
    system(command);
    free  (command);
  //g_string_free(command, TRUE);
}


/* unimplemented */



// handles POST /stop requests
// corresponds to on_video_sto
void airplay_video_stop(airplay_video_t *airplay_video, const char *session_id) {

}


// handles "POST /rate" requests
// equivalen code is service/casting_media_data_store: pro
void airplay_video_rate(airplay_video_t *airplay_video, const char *session_id, double rate) {

}


//handles "POST /scrub" reuests
void airplay_video_scrub( airplay_video_t *airplay_video, const char *session_id, double scrub_position) {
}


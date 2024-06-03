/*
 * Copyright (c) 2019 dsafa22, All Rights Reserved.
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
 *==================================================================
 * modified from raop_rtp_mirror.c under the above Copyright by
 *  by fduncanh 2024
 */

#include "airplay_video.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <stdbool.h>



#include "raop.h"
#include "netutils.h"
#include "compat.h"
#include "logger.h"
#include "byteutils.h"
#include "airplay_buffer.h"
#include "stream.h"
#include "utils.h"
#include "plist/plist.h"

struct airplay_video_s {
    logger_t *logger;
    raop_callbacks_t callbacks;
    raop_conn_t *conn;

    /* Remote address as sockaddr */
    struct sockaddr_storage remote_saddr;
    socklen_t remote_saddr_len;

    /* MUTEX LOCKED VARIABLES START */
    /* These variables only edited mutex locked */
    int running;
    int joined;

    int flush;
    thread_handle_t thread_mirror;
    mutex_handle_t run_mutex;

    /* MUTEX LOCKED VARIABLES END */
    int video_data_sock;

    unsigned short video_event_lport;

};


airplay_video_t *airplay_video_init(logger_t *logger, airplay_callbacks_t *callbacks, raop_conn_t *conn)
{
    airplay_video_t *airplay_video;

    assert(logger);
    assert(callbacks);

    airplay_video = calloc(1, sizeof(airplay_video_t));
    if (!airplay_video) {
        return NULL;
    }
    airplay_video->logger = logger;

    memcpy(&airplay_video->callbacks, callbacks, sizeof(raop_callbacks_t));
    airplay_video->buffer = video_buffer_init(logger);
    if (!airplay_video->buffer) {
        free(airplay_video);
        return NULL;
    }
    }
    airplay_video->running = 0;
    airplay_video->joined = 1;
    airplay_video->flush = NO_FLUSH;

    MUTEX_CREATE(airplay_video->run_mutex);
    return airplay_video;
}


static THREAD_RETVAL
airplay_video_thread(void *arg)
{
    airplay_video_t *airplay_video = arg;

        MUTEX_LOCK(airplay_video->run_mutex);
        if (!airplay_video->running) {
            MUTEX_UNLOCK(airplay_video->run_mutex);
            logger_log(airplay_video->logger, LOGGER_INFO, "airplay_video->running is no longer true");
            break;
        }
        MUTEX_UNLOCK(airplay_video->run_mutex);


        }


    return 0;
}


void
airplay_video_start(airplay_video_t *airplay_video, unsigned short *mirror_data_lport,
                      uint8_t show_client_FPS_data)
{
    logger_log(airplay_video->logger, LOGGER_INFO, "airplay_video starting mirroring");
    int use_ipv6 = 0;

    assert(airplay_video);
    assert(mirror_data_lport);
    airplay_video->show_client_FPS_data = show_client_FPS_data;

    MUTEX_LOCK(airplay_video->run_mutex);
    if (airplay_video->running || !airplay_video->joined) {
        MUTEX_UNLOCK(airplay_video->run_mutex);
        return;
    }

    if (airplay_video->remote_saddr.ss_family == AF_INET6) {
        use_ipv6 = 1;
    }
    //use_ipv6 = 0;
     
    airplay_video->mirror_data_lport = *mirror_data_lport;
    if (airplay_video_init_sockets(airplay_video, use_ipv6) < 0) {
        logger_log(airplay_video->logger, LOGGER_ERR, "airplay_video initializing sockets failed");
        MUTEX_UNLOCK(airplay_video->run_mutex);
        return;
    }
    *mirror_data_lport = airplay_video->mirror_data_lport;

    /* Create the thread and initialize running values */
    airplay_video->running = 1;
    airplay_video->joined = 0;

    THREAD_CREATE(airplay_video->thread_mirror, airplay_video_thread, airplay_video);
    MUTEX_UNLOCK(airplay_video->run_mutex);
}

void airplay_video_stop(airplay_video_t *airplay_video) {
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

    if (airplay_video->mirror_data_sock != -1) {
        closesocket(airplay_video->mirror_data_sock);
        airplay_video->mirror_data_sock = -1;
    }

    /* Join the thread */
    THREAD_JOIN(airplay_video->thread_mirror);

    /* Mark thread as joined */
    MUTEX_LOCK(airplay_video->run_mutex);
    airplay_video->joined = 1;
    MUTEX_UNLOCK(airplay_video->run_mutex);
}

void airplay_video_play_video(char * session_id, char *location, float 
  char ffplay_cmd[] = "nohup ffplay ";
    if (casting_data->video_cmd) {
      free (casting_data->video_cmd);
    }
  size_t len = strlen(ffplay) + strlen(casting_data->location) + 2;
  casting_data->video_cmd = (char *) calloc(len + 1, sizeof(char));
  snprint(casting_data->video_cmd, len, %s%s &", ffplay_cmd, casting_data->location);
  system(casting_data->video_cmd);
}

void airplay_video_destroy(void * ptr) {
  airplay_video = (airplay_video_t * (ptr)
    
    if (airplay_video) {
      	airplay-videoconn->airplay_video = NULL;
        airplay_video_stop(airplay_video);
        MUTEX_DESTROY(airplay_video->run_mutex);
        casting_data_destroy(airplay_video->casting_data);
	free(airplay_video);

    }
}

void casting_data_destroy(casting_data_t *casting_data) {
    if (casting_data) {
        if (casting_data->session_id) {
        free(casting_data->session_id);
    }
    if (casting_data->uuid) {
        free(casting_data->uuid);
    }
    if (casting_data->location) {
        free(casting_data->location);
    }
    if (casting_data->scheme) {
        free(casting_data->scheme);
    }
    free(casting_data);
  }
} 


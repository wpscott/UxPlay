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

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#ifdef _WIN32
#define CAST (char *)
#else
#define CAST
#endif

#include "airplay_video.h"
#include "threads.h"
#include "compat.h"
#include "netutils.h"
#include "byteutils.h"
#include "utils.h"


/*
playback_info_t *airplay_video_acquire_playback_info(const char *session_id);
void airplay_media_reset();
void airplay_video_stop(const char *session_id);
void airplay_video_rate(const char *session_id, double rate);
void airplay_video_play(const char *session_id, char *location, double start_position);
void airplay_video_scrub(const char *session_id, double scrub_position);
int query_media_data(const char *url, char **response_data);
char * airplay_process_media(char * fcup_response_url, char *  fcup_response_data, int fcup_response_datalen, int request_id);

/* these return NULL when i exceeds max number of time ranges */
/*time_range_t *get_loaded_time_range(int i);
time_range_t *get_seekable_time_range(int i);



airplay_video_t *airplay_video_service_init(logger_t *logger, raop_callbacks_t *callbacks, raop_conn_t *conn,
                                    raop_t *raop, const char *remote, int remotelen);				    
void airplay_video_service_start(airplay_video_t *airplay_video);
void airplay_video_service_stop(airplay_video_t *airplay_video);
void airplay_video_service_destroy(void *airplay_video);
*/


static int
airplay_video_parse_remote(airplay_video_t *airplay_video, const char *remote, int remote_addr_len)
{
    int family;
    int ret;
    assert(raop_ntp);
    if (remote_addr_len == 4) {
        family = AF_INET;
    } else if (remote_addr_len == 16) {
        family = AF_INET6;
    } else {
        return -1;
    }
    logger_log(airplay_video->logger, LOGGER_DEBUG, "airplay_video parse remote ip = %s", remote);
    ret = netutils_parse_address(family, remote,
                                 &raop_ntp->remote_saddr,
                                 sizeof(raop_ntp->remote_saddr));
    if (ret < 0) {
        return -1;
    }
    airplay_video->remote_saddr_len = ret;
    return 0;
}

airplay_video_t *airplay_video_service_init(logger_t *logger, raop_callbacks_t *callbacks, raop_conn_t *conn,
                                            raop_t *raop, const char *remote, int ipv6, unsigned short port, const char *session_id) {

    assert(logger);
    assert(callbacks);
    assert(conn);
    assert(raop);
    assert(remote);
    assert(session_id);
 
    airplay_video_t airplay_video =  (airplay_video_t) calloc(1, sizeof(airplay_video_t));
    if (!airplay_video) {
        return NULL;
    }
    airplay_video->logger = logger;
    memcpy(&airplay_video->callbacks, callbacks, sizeof(raop_callbacks_t));
    airplay_video->raop = raop;
    airplay_video->conn = conn;

    size_t len = strlen(remote);
    airplay_video->remote = (char * ) malloc(len + 1);
    strncpy(airplay_video->remote, remote, len);
    (airplay_video->remote)[len] = '\0';

    airplay_video->ipv6 = ipv6;

    len = strlen(session_id);
    airplay_video->session_id = (char *) malloc(len + 1);
    strncpy(airplay_video->session_id, remote, len);
    (airplay_video->session_id)[len] = '\0';
    
    char port_str[12];
    snprintf(port_str, sizeof(port_str),"%u", port);
    char prefix[] = "http://localhost:";
    len = strlen(prefix) + strlen(port_str);
    airplay_video->hls_prefix = (char * ) malloc(len + 1);
    snprintf(airplay_video->hls_prefix, len, "%s%s", prefix, port_str);
    (airplay_video->hls_prefix)[len] = '\0';
    
/* do we need this?  or should remote and remote_addr_len be removed from auguments  ? */
/*
    if (airplay_video_parse_remote(airplay_video, remote, remote_addr_len) < 0) {
        free(airplay_video);
        return NULL;
    }

    // Set port on the remote address struct   (this was for the ntp UDP timing port, but we already have a socket 
    ((struct sockaddr_in *) &raop_ntp->remote_saddr)->sin_port = htons(timing_rport);
*/
    airplay_video->running = 0;
    airplay_video->joined = 1;


    MUTEX_CREATE(airplay_video->run_mutex);
    MUTEX_CREATE(airplay_video->wait_mutex);
    COND_CREATE(airplay_video->wait_cond);
    return raop_ntp;
}

void
airplay_video_destroy(void * opaque)
{
    airplay_video_t *airplay_video = (airplay_video_t *) opaque;
        if (raop_ntp) {
        airplay_video_stop(airplay_video);
        MUTEX_DESTROY(airplay_video->run_mutex);
        MUTEX_DESTROY(airplay_video->wait_mutex);
        COND_DESTROY(airplay_video->wait_cond);
        MUTEX_DESTROY(airplay_video->sync_params_mutex);
        free(airplay_video_ntp);
    }
}
/*
unsigned short raop_ntp_get_port(raop_ntp_t *raop_ntp) {
    return raop_ntp->timing_lport;
}

static int
raop_ntp_init_socket(raop_ntp_t *raop_ntp, int use_ipv6)
{
    assert(raop_ntp);
    unsigned short tport = raop_ntp->timing_lport;
    int tsock = netutils_init_socket(&tport, use_ipv6, 1);

    if (tsock == -1) {
        goto sockets_cleanup;
    }

    // We're calling recvfrom without knowing whether there is any data, so we need a timeout

    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 300000;
    if (setsockopt(tsock, SOL_SOCKET, SO_RCVTIMEO, CAST &tv, sizeof(tv)) < 0) {
        goto sockets_cleanup;
    }

    // Set socket descriptors
    raop_ntp->tsock = tsock;

    // Set port values
    raop_ntp->timing_lport = tport;
    logger_log(raop_ntp->logger, LOGGER_DEBUG, "raop_ntp local timing port socket %d port UDP %d", tsock, tport);
    return 0;

    sockets_cleanup:
    if (tsock != -1) closesocket(tsock);
    return -1;
}

static void
raop_ntp_flush_socket(int fd)
{
#ifdef _WIN32
#define IOCTL ioctlsocket
    u_long bytes_available = 0;
#else
#define IOCTL ioctl
    int bytes_available = 0;
#endif
    while (IOCTL(fd, FIONREAD, &bytes_available) == 0 && bytes_available > 0)
    {
        // We are guaranteed that we won't block, because bytes are available.
        // Read 1 byte. Extra bytes in the datagram will be discarded.
        char c;
        int result = recvfrom(fd, &c, sizeof(c), 0, NULL, NULL);
        if (result < 0)
        {
            break;
        }
    }
}

*/

static THREAD_RETVAL
airplay_video_thread(void *arg)
{
    airplay_video_t *airplay_video = arg;
    assert(airplay_video);

    bool logger_debug = (logger_get_level(airplay_video->logger) >= LOGGER_DEBUG);
      
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
        MUTEX_UNLOCK(raop_ntp->wait_mutex);
    }

    // Ensure running reflects the actual state
    MUTEX_LOCK(airplay_video->run_mutex);
    airplay_video->running = false;
    MUTEX_UNLOCK(airplay->run_mutex);

    logger_log(airplay_video->logger, LOGGER_DEBUG, "airplay_video exiting thread");
    }
    return 0;
}

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
    MUTEX_UNLOCK(raop_ntp->run_mutex);
}

void
airplay_video_service_stop(airplay_video_t *airplay_video)
{
    assert(airplay_video);

    /* Check that we are running and thread is not
     * joined (should never be while still running) */
    MUTEX_LOCK(airplay_video->run_mutex);
    if (!airplay_video->running || airplay->joined) {
        MUTEX_UNLOCK(airplay_video->run_mutex);
        return;
    }
    airplay->running = 0;
    MUTEX_UNLOCK(airplay->run_mutex);

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

char * adjust_location(const char *playback_location) {
  /* reforms the HLS location */
  /* creates a char *: that must be freed after use */
  const char *address = strstr(playback_location,"://");
  const char prefix[] = "http://localhost:";
  char port_string[6];
  if (!address) {
    return NULL;
  }
  char *ptr = strstr(address, "://localhost/");
  if (ptr) {
    address += 12;
  } else {
    ptr = strstr(address, "://127.0.0.1/");
    if (ptr) {
      address += 12;
    } else {
      return NULL;
    }
  }
  char * lo = malloc(

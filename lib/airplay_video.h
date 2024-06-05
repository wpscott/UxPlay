/*
 * Copyright (c) 2024 fduncanh, All Rights Reserved.
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
  */

#ifndef AIRPLAY_VIDEO_H
#define AIRPLAY_VIDEO_H

#include <stdint.h>
#include <stdbool.h>
#include "raop.h"
#include "logger.h"

typedef struct time_range_s {
  double start;
  double duration;
} time_range_t;

typedef struct airplay_video_s {
  raop_t raop;
  logger_t *logger;
  raop_callbacks_t callbacks;
  raop_conn_t conn;
  char * session_id;
  thread_handle_t thread;
  mutex_handle_t run_mutex;
} airplay_video_t;

typedef struct playback_info_s {
  char * uuid;
  uint32_t stallCount;
  double duration;
  float position;
  double rate;
  bool readyToPlay;
  bool playbackBufferEmpty;
  bool playbackBufferFull;
  bool playbackLikelyToKeepUp;
  //  time_range_array_t *loadedTimeRanges;
  //time_range_array_t *seekableTimeRanges;
} playback_info_t;


playback_info_t *airplay_video_acquire_playback_info(const char *session_id);
void airplay_media_reset();
void airplay_video_stop(const char *session_id);
void airplay_video_rate(const char *session_id, double rate);
void airplay_video_play(const char *session_id, char *location, double start_position);
void airplay_video_scrub(const char *session_id, double scrub_position);
int query_media_data(const char *url, char **response_data);
char * airplay_process_media(char * fcup_response_url, char *  fcup_response_data, int fcup_response_datalen, int request_id);

/* these return NULL when i exceeds max number of time ranges */
time_range_t *get_loaded_time_range(int i);
time_range_t *get_seekable_time_range(int i);



airplay_video_t *airplay_video_service_init(logger_t *logger, raop_callbacks_t *callbacks, raop_conn_t *conn,
                                    raop_t *raop, const char *remote, int remotelen);				    
void airplay_video_service_start(airplay_video_t *airplay_video);
void airplay_video_service_stop(airplay_video_t *airplay_video);
void airplay_video_service_destroy(void *airplay_video);
#endif //AIRPLAY_VIDEO_H

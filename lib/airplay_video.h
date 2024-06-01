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

typedef struct time_range_array_s {
  int num_time_ranges;
  int max_num_time_ranges;
  time_range_t *time_ranges;
} time_ranges_array_t;


typedef struct playback_info_s {
  char * uuid;
  unit32_t stallCount;
  double duration;
  float position;
  double rate;
  bool readyToPlay;
  bool playbackBufferEmpty;
  bool playbackBufferFull;
  bool playbackLikelyToKeepUp;
  time_range_array_t loadedTimeRanges;
  time_range_array_t seekableTimeRanges;
} playback_info_t;


typedef struct airplay_video_s {
  raop_r raop;
  raop_conn_t conn;
  char * session_id;
}

typedef struct casting_data_s {
    char *session_id;
    char *uuid;
    char *location;
    char *scheme;
    float start_pos_ms;
    char *video_cmd;
} casting_data_t;

void casting_data_destroy(casting_data_t *casting_data);

typedef struct airplay_video_s airplay_video_t;

airplay_video_t *airplay_video_init(logger_t *logger, raop_callbacks_t *callbacks, raop_conn_t *conn,
                                    raop_t *raop, const char *remote, int remotelen);				    
void airplay_video_start(airplay_video_t *airplay_video, casting_data_t *casting_data);
void airplay_video_stop(airplay_video_t *airplay_video);
void airplay_video_destroy(void *airplay_video);
#endif //AIRPLAY_VIDEO_H

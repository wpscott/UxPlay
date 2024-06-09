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

typedef struct airplay_video_s airplay_video_t;

int airplay_video_acquire_playback_info(airplay_video_t *airplay_video, const char *session_id, char **plist_xml);
void airplay_media_reset(airplay_video_t *airplay_video);
void airplay_video_stop(airplay_video_t *airplay_video, const char *session_id);
void airplay_video_rate(airplay_video_t *airplay_video, const char *session_id, double rate);
void airplay_video_play(airplay_video_t *airplay_video, const char *session_id, const char *location, double start_position);
void airplay_video_scrub(airplay_video_t *airplay_video, const char *session_id, double scrub_position);
int query_media_data(airplay_video_t *airplay_video, const char *url, char **response_data);
char *airplay_process_media_data(airplay_video_t *airplay_video, char *fcup_response_url, char *fcup_response_data,
                            int fcup_response_datalen, int request_id);

void airplay_video_service_start(airplay_video_t *airplay_video);
void airplay_video_service_stop(airplay_video_t *airplay_video);
void airplay_video_service_destroy(airplay_video_t *airplay_video);
#endif //AIRPLAY_VIDEO_H

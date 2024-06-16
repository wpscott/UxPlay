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

void airplay_video_stop(airplay_video_t *airplay_video, const char *session_id);
void airplay_video_rate(airplay_video_t *airplay_video, const char *session_id, double rate);
void airplay_video_play(airplay_video_t *airplay_video, const char *session_id, const char *location, double start_position);
void airplay_video_scrub(airplay_video_t *airplay_video, const char *session_id, double scrub_position);

void airplay_video_service_start(airplay_video_t *airplay_video);
void airplay_video_service_stop(airplay_video_t *airplay_video);
void airplay_video_service_destroy(airplay_video_t *airplay_video);

//  C wrappers for c++ class MediaDataStore
//create the media_data_store, return a pointer to it.
void* media_data_store_create(void *conn_opaque, uint16_t port);

//delete the media_data_store
void media_data_store_destroy(void *media_data_store);

// called by the POST /action handler:
char *process_media_data(void *media_data_store, const char *url, const char *data, int datalen);

//called by the POST /play handler
bool request_media_data(void *media_data_store, const char *primary_url, const char * session_id);

//called by airplay_video_media_http_connection::get_handler:   &path = req.uri)
int query_media_data(void *media_data_store, const char *url, const char **media_data);

//called by the post_stop_handler:
void media_data_store_reset(void *media_data_store);

const char *adjust_primary_uri(void *media_data_store, const char *url);
  
// set and get session_id_, start_pos_in_ms_, and playback_uuid_
bool check_session_id(void *media_data_store, const char* session_id);
void set_session_id(void *media_data_store, const char *session_id);
void set_playback_uuid(void *media_data_store, const char *playback_uuid);
float get_start_pos_in_ms(void *media_data_store);
void set_start_pos_in_ms(void *media_data_store, float start_pos_in_ms);
int get_fcup_request_id(void *media_data_store);
void set_socket_fd(void *media_data_store, int socket_fd);
int get_socket_fd(void *media_data_store);

#endif //AIRPLAY_VIDEO_H

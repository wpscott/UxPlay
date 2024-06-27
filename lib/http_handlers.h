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

/* this file is part of raop.c and should not be included in any other file */

#include "airplay_video.h"
#include "fcup_request.h"

static void
http_handler_server_info(raop_conn_t *conn, http_request_t *request, http_response_t *response,
                         char **response_data, int *response_datalen)  {

    assert(conn->raop->dnssd);
    int hw_addr_raw_len = 0;
    const char *hw_addr_raw = dnssd_get_hw_addr(conn->raop->dnssd, &hw_addr_raw_len);

    char *hw_addr = calloc(1, 3 * hw_addr_raw_len);
    //int hw_addr_len =
    utils_hwaddr_airplay(hw_addr, 3 * hw_addr_raw_len, hw_addr_raw, hw_addr_raw_len);

    plist_t r_node = plist_new_dict();

    /* first 12 AirPlay features bits (R to L): 0x27F = 0010 0111 1111
     * Only bits 0-6 and bit 9  are set:
     * 0. video supported
     * 1. photo supported
     * 2. video protected wirh FairPlay DRM
     * 3. volume control supported for video
     * 4. HLS supported
     * 5. slideshow supported
     * 6. (unknown)
     * 9. audio supported.
     */
    plist_t features_node = plist_new_uint(0x27F); 
    plist_dict_set_item(r_node, "features", features_node);

    plist_t mac_address_node = plist_new_string(hw_addr);
    plist_dict_set_item(r_node, "macAddress", mac_address_node);

    plist_t model_node = plist_new_string(GLOBAL_MODEL);
    plist_dict_set_item(r_node, "model", model_node);

    plist_t os_build_node = plist_new_string("12B435");
    plist_dict_set_item(r_node, "osBuildVersion", os_build_node);

    plist_t protovers_node = plist_new_string("1.0");
    plist_dict_set_item(r_node, "protovers", protovers_node);

    plist_t source_version_node = plist_new_string(GLOBAL_VERSION);
    plist_dict_set_item(r_node, "srcvers", source_version_node);

    plist_t vv_node = plist_new_uint(strtol(AIRPLAY_VV, NULL, 10));
    plist_dict_set_item(r_node, "vv", vv_node);

    plist_t device_id_node = plist_new_string(hw_addr);
    plist_dict_set_item(r_node, "deviceid", device_id_node);

    plist_to_xml(r_node, response_data, (uint32_t *) response_datalen);

    //assert(*response_datalen == strlen(*response_data));

    /* last character (at *response_data[response_datalen - 1]) is  0x0a = '\n'
     * (*response_data[response_datalen] is '\0').
     * apsdk removes the last "\n" by overwriting it with '\0', and reducing response_datalen by 1. 
     * TODO: check if this is necessary  */
    
    plist_free(r_node);
    http_response_add_header(response, "Content-Type", "text/x-apple-plist+xml");
    free(hw_addr);
    
    /* initialize the aiplay video service */
    const char *session_id = http_request_get_header(request, "X-Apple-Session-ID");

    conn->airplay_video =  (void *) airplay_video_service_init(conn, conn->raop, conn->raop->port, session_id);

    logger_log(conn->raop->logger, LOGGER_DEBUG, "media_data_store accessible at %p",
               get_media_data_store(conn->raop));
}

static void
http_handler_get_property(raop_conn_t *conn, http_request_t *request, http_response_t *response,
                          char **response_data, int *response_datalen) {
    const char *url = http_request_get_url(request);
    const char *property = url + strlen("getProperty?");
    logger_log(conn->raop->logger, LOGGER_DEBUG, "http_handler_get_property: %s (unhandled)", property);
}

static void
http_handler_scrub(raop_conn_t *conn, http_request_t *request, http_response_t *response,
                   char **response_data, int *response_datalen) {
    const char *url = http_request_get_url(request);
    const char *data = strstr(url, "?");
    float scrub_position = 0.0f;
    if (data) {
        data++;
	const char *position = strstr(data, "=") + 1;
	char *end;
	double value = strtod(position, &end);
	if (end && end != position) {
	  scrub_position = (float) value;
	  logger_log(conn->raop->logger, LOGGER_DEBUG, "http_handler_scrub: got position = %.6f",
                     scrub_position);	  
	}
    }
    conn->raop->callbacks.on_video_scrub(conn->raop->callbacks.cls, scrub_position);
}

static void
http_handler_rate(raop_conn_t *conn, http_request_t *request, http_response_t *response,
                  char **response_data, int *response_datalen) {

    const char *url = http_request_get_url(request);
    const char *data = strstr(url, "?");
    float rate_value = 0.0f;
    if (data) {
        data++;
	const char *rate = strstr(data, "=") + 1;
	char *end;
	double value = strtod(rate, &end);
	if (end && end != rate) {
	  rate_value = (float) value;
          set_playback_info_item(conn->airplay_video, "rate", 0, &rate_value);
	  logger_log(conn->raop->logger, LOGGER_DEBUG, "http_handler_rate: got rate = %.6f", rate_value);	  
	}
    }
    conn->raop->callbacks.on_video_rate(conn->raop->callbacks.cls, rate_value);

}

static void
http_handler_fpsetup2(raop_conn_t *conn, http_request_t *request, http_response_t *response,
                      char **response_data, int *response_datalen) {
    logger_log(conn->raop->logger, LOGGER_WARNING, "client HTTP request POST fp-setup2 is unhandled");
    http_response_add_header(response, "Content-Type", "application/x-apple-binary-plist");
    int req_datalen;
    const unsigned char *req_data = (unsigned char *) http_request_get_data(request, &req_datalen);
    logger_log(conn->raop->logger, LOGGER_ERR, "only FairPlay version 0x03 is implemented, version is 0x%2.2x",
               req_data[4]);
    http_response_destroy(response);
    response = http_response_init("HTTP/1.1", 421, "Misdirected Request");
}

// called by http_handler_playback_info to respond to a GET /playback_info request from the client.

int create_playback_info_plist_xml(playback_info_t *playback_info, char **plist_xml) {

    plist_t res_root_node = plist_new_dict();

    plist_t duration_node = plist_new_real(playback_info->duration);
    plist_dict_set_item(res_root_node, "duration", duration_node);

    plist_t position_node = plist_new_real(playback_info->position);
    plist_dict_set_item(res_root_node, "position", position_node);

    plist_t rate_node = plist_new_real(playback_info->rate);
    plist_dict_set_item(res_root_node, "rate", rate_node);

    /* should these be int or bool? */
    plist_t ready_to_play_node = plist_new_int(playback_info->ready_to_play);
    plist_dict_set_item(res_root_node, "readyToPlay", ready_to_play_node);

    plist_t playback_buffer_empty_node = plist_new_int(playback_info->playback_buffer_empty);
    plist_dict_set_item(res_root_node, "playbackBufferEmpty", playback_buffer_empty_node);

    plist_t playback_buffer_full_node = plist_new_int(playback_info->playback_buffer_full);
    plist_dict_set_item(res_root_node, "playbackBufferFull", playback_buffer_full_node);

    plist_t playback_likely_to_keep_up_node = plist_new_int(playback_info->playback_likely_to_keep_up);
    plist_dict_set_item(res_root_node, "playbackLikelyToKeepUp", playback_likely_to_keep_up_node);

    plist_t loaded_time_ranges_node = plist_new_array();

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

    int len;
    plist_to_xml(res_root_node, plist_xml, (uint32_t *) &len);
    plist_free(res_root_node);

    /* remove a final /n (suggested by apsdk-public code):  is this necessary when using plist_to_xml? */
    len --;
    (*plist_xml)[len] = '\0';
    
    return len;
}

// this adds a time range (duration, start) of time-range_type = "loadedTimeRange" or
// "seekableTimeRange" to the playback_info struct, and increments the appropriate counter by 1. 
// Not more than MAX_TIME_RANGES of a give type may be added.
// returns 0 for success, -1 for failure.

int add_playback_info_time_range(playback_info_t *playback_info, const char *time_range_type,
                                double duration, double start) {
    time_range_t *time_range;
    int *time_range_num;
     if (!strcmp(time_range_type, "loadedTimeRange")) {
        time_range_num = &(playback_info->num_loaded_time_ranges);
        time_range = (time_range_t *) &playback_info->loadedTimeRanges[*time_range_num];
    } else if (!strcmp(time_range_type, "seekableTimeRange")) {
        time_range_num = &(playback_info->num_seekable_time_ranges);
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

static void
http_handler_playback_info(raop_conn_t *conn, http_request_t *request, http_response_t *response,
                           char **response_data, int *response_datalen)
{
    logger_log(conn->raop->logger, LOGGER_DEBUG, "http_handler_playback_info");
    const char *session_id = http_request_get_header(request, "X-Apple-Session-ID");

    playback_info_t playback_info;
    playback_info.stallcount = 0;
    playback_info.duration = 0.0;
    playback_info.position =  0.0f;
    playback_info.ready_to_play = false;
    playback_info.playback_buffer_empty = true;
    playback_info.playback_buffer_full = false;
    playback_info.playback_likely_to_keep_up = true;
    playback_info.num_loaded_time_ranges = 0;
    playback_info.num_seekable_time_ranges = 0;

    conn->raop->callbacks.on_video_acquire_playback_info(conn->raop->callbacks.cls, &playback_info);
    add_playback_info_time_range(&playback_info, "loadedTimeRange", playback_info.duration, 0.0);
    add_playback_info_time_range(&playback_info, "seekableTimeRange", playback_info.duration, 0.0);

    *response_datalen =  create_playback_info_plist_xml(&playback_info, response_data);
    /* last character (at *response_data[response_datalen - 1]) is  0x0a = '\n'
     * (*response_data[response_datalen] is '\0').
     * apsdk removes the last "\n" by overwriting it with '\0', and reducing response_datalen by 1. 
     * TODO: check if this is necessary  */

    http_response_add_header(response, "Content-Type", "text/x-apple-plist+xml");
}

static void
http_handler_set_property(raop_conn_t *conn,
                      http_request_t *request, http_response_t *response,
			  char **response_data, int *response_datalen) {

    const char *url = http_request_get_url(request);
    const char *property = url + strlen("/setProperty?");
    logger_log(conn->raop->logger, LOGGER_DEBUG, "http_handler_set_property: %s", property);

    if (!strcmp(property, "reverseEndTime") ||
        !strcmp(property, "forwardEndTime") ||
        !strcmp(property, "actionAtItemEnd")) {
        logger_log(conn->raop->logger, LOGGER_DEBUG, "property %s is known but unhandled", property);

        plist_t errResponse = plist_new_dict();
        plist_t errCode = plist_new_uint(0);
        plist_dict_set_item(errResponse, "errorCode", errCode);
        plist_to_xml(errResponse, response_data, (uint32_t *) response_datalen);
        plist_free(errResponse);
        http_response_add_header(response, "Content-Type", "text/x-apple-plist+xml");
    } else {
        logger_log(conn->raop->logger, LOGGER_DEBUG, "property %s is unknown, unhandled", property);      
        http_response_add_header(response, "Content-Length", "0");
    }
}

static void
http_handler_reverse(raop_conn_t *conn, http_request_t *request, http_response_t *response,
                     char **response_data, int *response_datalen) {

    /* get http socket for send */
    int socket_fd = httpd_get_connection_socket (conn->raop->httpd, (void *) conn);
    if (socket_fd < 0) {
        logger_log(conn->raop->logger, LOGGER_ERR, "fcup_request failed to retrieve socket_fd from httpd");
        /* shut down connection? */
    }
    
    const char *purpose = http_request_get_header(request, "X-Apple-Purpose");
    const char *connection = http_request_get_header(request, "Connection");
    const char *upgrade = http_request_get_header(request, "Upgrade");
    logger_log(conn->raop->logger, LOGGER_INFO, "client requested reverse connection: %s; purpose: %s  \"%s\"",
               connection, upgrade, purpose);

    httpd_set_connection_type(conn->raop->httpd, (void *) conn, CONNECTION_TYPE_PTTH);
    int type_PTTH = httpd_count_connection_type(conn->raop->httpd, CONNECTION_TYPE_PTTH);

    if (type_PTTH == 1) {
        logger_log(conn->raop->logger, LOGGER_DEBUG, "will use socket %d for %s connections", socket_fd, purpose);
        http_response_destroy(response);
        response = http_response_init("HTTP/1.1", 101, "Switching Protocols");
	http_response_add_header(response, "Connection", "Upgrade");
	http_response_add_header(response, "Upgrade", "PTTH/1.0");

    } else {
        logger_log(conn->raop->logger, LOGGER_ERR, "multiple TPPH connections (%d) are forbidden", type_PTTH );
    }    
}

// handlers that use the media_data_store (c++ code)

static void
http_handler_stop(raop_conn_t *conn, http_request_t *request, http_response_t *response,
                  char **response_data, int *response_datalen) {
    logger_log(conn->raop->logger, LOGGER_INFO, "client HTTP request POST stop");
    void *media_data_store = get_media_data_store(conn->raop);

    if (media_data_store) {
        media_data_store_reset(media_data_store);
    } else {
        logger_log(conn->raop->logger, LOGGER_DEBUG, "media_data_store not found");
    }
    conn->raop->callbacks.on_video_stop(conn->raop->callbacks.cls);
}

static void
http_handler_action(raop_conn_t *conn, http_request_t *request, http_response_t *response,
                    char **response_data, int *response_datalen) {

    bool data_is_plist = false;
    plist_t req_root_node = NULL;
    char *fcup_response_data = NULL;
    char *fcup_response_url = NULL;
    uint64_t uint_val;
    void *media_data_store = NULL;
    int request_id = 0;
    int fcup_response_statuscode = 0;
    
    media_data_store = get_media_data_store(conn->raop);
    if (!media_data_store) {
        logger_log(conn->raop->logger, LOGGER_ERR, "media_data_store not found");
        return;
    }

    const char* session_id = http_request_get_header(request, "X-Apple-Session-ID");
    if (!session_id) {
        logger_log(conn->raop->logger, LOGGER_ERR, "Play request had no X-Apple-Session-ID");
        goto post_action_error;
    }    
    const char *apple_session_id = get_apple_session_id(conn->airplay_video);
    if (strcmp(session_id, apple_session_id)){
        logger_log(conn->raop->logger, LOGGER_ERR, "X-Apple-Session-ID has changed:\n  was:\"%s\"\n  now:\"%s\"",
                   apple_session_id, session_id);
        goto post_action_error;
    }

    /* verify that this reponse contains a binary plist*/
    char *header_str = NULL;
    http_request_get_header_string(request, &header_str);
    logger_log(conn->raop->logger, LOGGER_INFO, "request header: %s", header_str);
    data_is_plist = (strstr(header_str,"apple-binary-plist") != NULL);
    free(header_str);
    if (!data_is_plist) {
        logger_log(conn->raop->logger, LOGGER_INFO, "POST /action: did not receive expected plist from client");	
        goto post_action_error;
    }

    /* extract the root_node  plist */
    int request_datalen = 0;
    const char *request_data = http_request_get_data(request, &request_datalen);
    if (request_datalen == 0) {
        logger_log(conn->raop->logger, LOGGER_INFO, "POST /action: did not receive expected plist from client");	
        goto post_action_error;
    }
    plist_from_bin(request_data, request_datalen, &req_root_node);

    /* determine type of data */
    plist_t req_type_node = plist_dict_get_item(req_root_node, "type");
    if (!PLIST_IS_STRING(req_type_node)) {
      goto post_action_error;
    }

    /* three possible types are known */
    char *type = NULL;
    int action_type = 0;
    plist_get_string_val(req_type_node, &type);
    logger_log(conn->raop->logger, LOGGER_INFO, "action type is %s", type);
    if (strstr(type, "unhandledURLResponse")) {
      action_type =  1;
    } else if (strstr(type, "playlistInsert")) {
      action_type = 2;
    } else if (strstr(type, "playlistRemove")) {
      action_type = 3;
    } 
    free (type);

    plist_t req_params_node = NULL;
    switch (action_type) {
    case 1:
      req_params_node = plist_dict_get_item(req_root_node, "params");
      if (PLIST_IS_DICT (req_params_node)) {
	goto handle_fcup;
      }
      goto post_action_error;
    case 2:
      logger_log(conn->raop->logger, LOGGER_INFO, "unhandled action type playlistInsert (add new playback)");
      goto finish;
    case 3:
      logger_log(conn->raop->logger, LOGGER_INFO, "unhandled action type playlistRemove (stop playback)");
      goto finish;
    default:
      logger_log(conn->raop->logger, LOGGER_INFO, "unknown action type (unhandled)"); 
      goto finish;
    }

 handle_fcup:

    /* handling type "unhandledURLResponse" (case 1)*/
    uint_val = 0;
    int fcup_response_datalen = 0;

    plist_t plist_fcup_response_statuscode_node = plist_dict_get_item(req_params_node,
                                                                      "FCUP_Response_StatusCode");
    if (plist_fcup_response_statuscode_node) {
        plist_get_uint_val(plist_fcup_response_statuscode_node, &uint_val);
        fcup_response_statuscode = (int) uint_val;
        uint_val = 0;
        logger_log(conn->raop->logger, LOGGER_INFO, "FCUP_Response_StatusCode = %d",
                   fcup_response_statuscode);
    }

    plist_t plist_fcup_response_requestid_node = plist_dict_get_item(req_params_node,
                                                                     "FCUP_Response_RequestID");
    if (plist_fcup_response_requestid_node) {
        plist_get_uint_val(plist_fcup_response_requestid_node, &uint_val);
        request_id = (int) uint_val;
        uint_val = 0;
        logger_log(conn->raop->logger, LOGGER_DEBUG, "FCUP_Response_RequestID =  %d", request_id);
    }

    plist_t plist_fcup_response_url_node = plist_dict_get_item(req_params_node, "FCUP_Response_URL");
    if (plist_fcup_response_url_node) {
        plist_get_string_val(plist_fcup_response_url_node, &fcup_response_url);
        if (!fcup_response_url) {
            goto post_action_error;
        }
        logger_log(conn->raop->logger, LOGGER_DEBUG, "FCUP_Response_URL =  %s", fcup_response_url);
    }

	
    plist_t plist_fcup_response_data_node = plist_dict_get_item(req_params_node, "FCUP_Response_Data");
    if (!PLIST_IS_DATA(plist_fcup_response_data_node)){
        goto post_action_error;
    } else {
        plist_get_data_val(plist_fcup_response_data_node, &fcup_response_data, &uint_val);
        fcup_response_datalen = (int) uint_val;
        uint_val = 0;
	
	free (fcup_response_data);
        if (!fcup_response_datalen) {
	    free (fcup_response_url);
	    goto post_action_error;
        }
    } 

    char *playback_location = process_media_data(media_data_store, fcup_response_url,
                                                 fcup_response_data, fcup_response_datalen);

    /* play, if location != NULL */
    if (playback_location) {  
        conn->raop->callbacks.on_video_play(conn->raop->callbacks.cls, playback_location,
					    get_start_position_seconds(conn->airplay_video));
        free (playback_location);
    }

 finish:
    plist_free(req_root_node);
    return;

 post_action_error:;
    http_response_destroy(response);
    response = http_response_init("HTTP/1.1", 400, "Bad Request");

    if (req_root_node)  {
      plist_free(req_root_node);
    }
}

static void
http_handler_play(raop_conn_t *conn, http_request_t *request, http_response_t *response,
                      char **response_data, int *response_datalen) {

    char* playback_location = NULL;
    plist_t req_root_node = NULL;
    float start_position_seconds = 0.0f;
    bool data_is_binary_plist = false;
    bool data_is_text = false;
    bool data_is_octet = false;
    void *media_data_store = NULL;
    bool ret;

    logger_log(conn->raop->logger, LOGGER_DEBUG, "http_handler_play");
    media_data_store = get_media_data_store(conn->raop);
    if (!media_data_store) {
      logger_log(conn->raop->logger, LOGGER_ERR, "media_data_store not found, conn = %p", conn);

        return;
    }

    const char* session_id = http_request_get_header(request, "X-Apple-Session-ID");
    if (!session_id) {
        logger_log(conn->raop->logger, LOGGER_ERR, "Play request had no X-Apple-Session-ID");
        goto play_error;
    }
    const char *apple_session_id = get_apple_session_id(conn->airplay_video);
    if (strcmp(session_id, apple_session_id)){
        logger_log(conn->raop->logger, LOGGER_ERR, "X-Apple-Session-ID has changed:\n  was:\"%s\"\n  now:\"%s\"",
                   apple_session_id, session_id);
        goto play_error;
    }
      
    int request_datalen = -1;    
    const char *request_data = http_request_get_data(request, &request_datalen);

    if (request_datalen > 0) {
        char *header_str = NULL;
        http_request_get_header_string(request, &header_str);
        logger_log(conn->raop->logger, LOGGER_INFO, "request header:\n%s", header_str);
	data_is_binary_plist = (strstr(header_str, "x-apple-binary-plist") != NULL);
	data_is_text = (strstr(header_str, "text/parameters") != NULL);
	data_is_octet = (strstr(header_str, "octet-stream") != NULL);
	free (header_str);
    }
    if (!data_is_text && !data_is_octet && !data_is_binary_plist) {
      goto play_error;
    }
    
    if (data_is_text) {
         logger_log(conn->raop->logger, LOGGER_ERR, "Play request Content is text (unsupported)");
	 goto play_error;
    }
    
    if (data_is_octet) {
         logger_log(conn->raop->logger, LOGGER_ERR, "Play request Content is octet-stream (unsupported)");
	 goto play_error;
    }
    
    if (data_is_binary_plist) {
        plist_from_bin(request_data, request_datalen, &req_root_node);

        plist_t req_uuid_node = plist_dict_get_item(req_root_node, "uuid");
        if (!req_uuid_node) {
            goto play_error;
        } else {
            char* playback_uuid = NULL;
            plist_get_string_val(req_uuid_node, &playback_uuid);
	    set_playback_uuid(conn->airplay_video, playback_uuid);
            free (playback_uuid);
	}

        plist_t req_content_location_node = plist_dict_get_item(req_root_node, "Content-Location");
        if (!req_content_location_node) {
            goto play_error;
        } else {
            plist_get_string_val(req_content_location_node, &playback_location);
        }

        plist_t req_start_position_seconds_node = plist_dict_get_item(req_root_node, "Start-Position-Seconds");
        if (!req_start_position_seconds_node) {
            logger_log(conn->raop->logger, LOGGER_INFO, "No Start-Position-Seconds in Play request");	    
         } else {
             double start_position = 0.0;
             plist_get_real_val(req_content_location_node, &start_position);
	     start_position_seconds = (float) start_position;
        }
	set_start_position_seconds(conn->airplay_video, (float) start_position_seconds);
    }

    ret = request_media_data(media_data_store, playback_location, apple_session_id);
    
    if (!ret) {
        /* normal play, not HLS: assume location is valid */
        logger_log(conn->raop->logger, LOGGER_INFO, "Play normal (non-HLS) video, location = %s", playback_location);
        //does the player want start position in secs or msecs ?
        conn->raop->callbacks.on_video_play(conn->raop->callbacks.cls, playback_location, start_position_seconds);
      }

    if (playback_location) {
        free (playback_location);
    }

    if (req_root_node) {
        plist_free(req_root_node);
    }
     return;

 play_error:;
    if (req_root_node) {
      plist_free(req_root_node);
    }
    logger_log(conn->raop->logger, LOGGER_ERR, "Couldn't find valid Plist Data for /play, Unhandled");
    http_response_destroy(response);
    response = http_response_init("HTTP/1.1", 400, "Bad Request");
}

static void
http_handler_hls(raop_conn_t *conn,  http_request_t *request, http_response_t *response,
                 char **response_data, int *response_datalen) {
    const char *method = http_request_get_method(request);
    assert (!strcmp(method, "GET"));
    const char *url = http_request_get_url(request);    
    void *media_data_store = NULL;    	    
    const char* upgrade = http_request_get_header(request, "Upgrade");
    if (upgrade) {
      //don't accept Upgrade: h2c request ?
      return;
    }
    media_data_store = get_media_data_store(conn->raop);
    if (!media_data_store) {
        logger_log(conn->raop->logger, LOGGER_ERR, "media_data_store not found");
        return;
    }

    *response_data = query_media_data(media_data_store, url, response_datalen);

    http_response_add_header(response, "Access-Control-Allow-Headers", "Content-type");
    http_response_add_header(response, "Access-Control-Allow-Origin", "*");
    const char *date;
    date = gmt_time_string();
    http_response_add_header(response, "Date", date);
    if (*response_datalen > 0) {
        http_response_add_header(response, "Content-Type", "application/x-mpegURL; charset=utf-8");
    } else if (*response_datalen == 0) {
      http_response_destroy(response);
      response = http_response_init("HTTP/1.1", 404, "Not Found");
    }
 }

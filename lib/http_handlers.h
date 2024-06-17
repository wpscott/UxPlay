/*
 * Copyright (c) 2022 fduncanh
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

typedef void (*hls_handler_t)(raop_conn_t *, http_request_t *,
                               http_response_t *, const char **, int *);

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

    conn->airplay_video =  (void *) airplay_video_service_init(conn->raop->logger, &(conn->raop->callbacks), conn,
                                                               conn->raop, conn->raop->port, session_id);

    logger_log(conn->raop->logger, LOGGER_DEBUG, "media_data_store accessible at %p", get_media_data_store(conn->raop));
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
    double scrub_position = 0.0;
    if (data) {
        data++;
	const char *position = strstr(data, "=") + 1;
	char *end;
	double value = strtod(position, &end);
	if (end && end != position) {
	  scrub_position = value;
	  logger_log(conn->raop->logger, LOGGER_DEBUG, "http_handler_scrub: got position = %f", scrub_position);	  
	}
    }

    const char *session_id = http_request_get_header(request, "X-Apple-Session-ID");
    airplay_video_scrub(conn->airplay_video, session_id, scrub_position);
}

static void
http_handler_rate(raop_conn_t *conn, http_request_t *request, http_response_t *response,
                  char **response_data, int *response_datalen) {

    const char *session_id = http_request_get_header(request, "X-Apple-Session-ID");
    assert(!check_session_id(conn->airplay_video, session_id));
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


}

static void
http_handler_fpsetup2(raop_conn_t *conn, http_request_t *request, http_response_t *response,
                      char **response_data, int *response_datalen) {
    logger_log(conn->raop->logger, LOGGER_WARNING, "client HTTP request POST fp-setup2 is unhandled");
    http_response_add_header(response, "Content-Type", "application/x-apple-binary-plist");
    int req_datalen;
    const unsigned char *req_data = (unsigned char *) http_request_get_data(request, &req_datalen);
    logger_log(conn->raop->logger, LOGGER_ERR, "only FairPlay version 0x03 is implemented, version is 0x%2.2x", req_data[4]);
    http_response_destroy(response);
    response = http_response_init("HTTP/1.1", 421, "Misdirected Request");
}


static void
http_handler_playback_info(raop_conn_t *conn,
                      http_request_t *request, http_response_t *response,
                      char **response_data, int *response_datalen)
{
    logger_log(conn->raop->logger, LOGGER_DEBUG, "http_handler_playback_info");
    const char *session_id = http_request_get_header(request, "X-Apple-Session-ID");
    
    *response_datalen  =  airplay_video_acquire_playback_info(conn->airplay_video, session_id, response_data);

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
        printf("%s", *response_data);
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


// handlers that use the media_data_store   (c++ code)

static void
http_handler_stop(raop_conn_t *conn, http_request_t *request, http_response_t *response,
                  char **response_data, int *response_datalen) {
    logger_log(conn->raop->logger, LOGGER_INFO, "client HTTP request POST stop");
    const char *session_id = http_request_get_header(request, "X-Apple-Session-ID");
    void *media_data_store = get_media_data_store(conn->raop);
    
    if (media_data_store) {
        media_data_store_reset(media_data_store);
    } else {
        logger_log(conn->raop->logger, LOGGER_DEBUG, "media_data_store not found");
    }
    airplay_video_stop(conn->airplay_video, session_id);
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

    media_data_store = get_media_data_store(conn->raop);
    if (!media_data_store) {
        logger_log(conn->raop->logger, LOGGER_ERR, "media_data_store not found");
        return;
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
    if (!strstr(type, "unhandledURLResponse")) {
      action_type =  1;
    } else if (!strstr(type, "playlistInsert")) {
      action_type = 2;
    } else if (!strstr(type, "playlistRemove")) {
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
      logger_log(conn->raop->logger, LOGGER_INFO, "unhandled action type playlistInsert");
      goto finish;
    case 3:
      logger_log(conn->raop->logger, LOGGER_INFO, "unhandled action type playlistRemove");
      goto finish;
    default:
      logger_log(conn->raop->logger, LOGGER_INFO, "unknown action type (unhandled)"); 
      goto finish;
    }
    
    handle_fcup:
    /* handling type "unhandledURLResponse" (case 1)*/
    uint_val = 0;
    int fcup_response_datalen = 0;

#if 0   // these entries were not used by apsdk
    int request_id = 0;
    int fcup_response_statuscode = 0;

    plist_t plist_fcup_response_statuscode_node = plist_dict_get_item(req_params_node, "FCUP_Response_StatusCode");
    plist_get_uint_val(plist_fcup_response_statuscode_node, &uint_val);
    fcup_response_statuscode = (int) uint_val;
    uint_val = 0;
    if (!fcup_response_statuscode) {
        logger_log(conn->raop->logger, LOGGER_INFO, "unhandledURLResponse with non-zero FCUP_Response_StatusCode = %d",
                   fcup_response_statuscode);
        goto post_action_error;
    }

    plist_t plist_fcup_response_requestid_node = plist_dict_get_item(req_params_node, "FCUP_Response_RequestID");    
    plist_get_uint_val(plist_fcup_response_requestid_node, &uint_val);
    request_id = (int) uint_val;
    uint_val = 0;
#endif

    
    plist_t plist_fcup_response_url_node = plist_dict_get_item(req_params_node, "FCUP_Response_URL");
    plist_get_string_val(plist_fcup_response_url_node, &fcup_response_url);
    if (!fcup_response_url) {
        goto post_action_error;
    }

    plist_t plist_fcup_response_data_node = plist_dict_get_item(req_params_node, "FCUP_Response_Data");
    if (!PLIST_IS_DATA(plist_fcup_response_data_node)){
        goto post_action_error;
    }
    plist_get_data_val(plist_fcup_response_data_node, &fcup_response_data, &uint_val);
    fcup_response_datalen = (int) uint_val;
    uint_val = 0;
    if (!fcup_response_datalen) {
      free (fcup_response_url);
      goto post_action_error;
    }

    printf("**************process_media_data**************\n");
    char *location = process_media_data(media_data_store, fcup_response_url, fcup_response_data, fcup_response_datalen);
    printf("********  process_media_data return location = \"%s\"  *************************\n", location);
    /* play, if location != NULL */
    if (location) {
      const char *session_id = http_request_get_header(request, "X-Apple-Session-ID");
      airplay_video_play(conn->airplay_video, session_id, location, get_start_pos_in_ms(media_data_store));
      free (location);
    }
    
 finish:
    plist_free(req_root_node);
    
    plist_t response_node = plist_new_dict();
    plist_t errorcode_node = plist_new_uint(0);
    plist_dict_set_item(response_node, "errorCode", errorcode_node);
    plist_to_xml(response_node, response_data, (uint32_t *) response_datalen);
    plist_free(response_node);
    http_response_add_header(response, "Content-Type", "application/x-apple-plist+xml");
    return;

 post_action_error:;
    http_response_destroy(response);
    response = http_response_init("HTTP/1.1", 400, "Bad Request");

    if (req_root_node)  {
      plist_free(req_root_node);
    }
}

static void
http_handler_hls(raop_conn_t *conn,  http_request_t *request, http_response_t *response,
			  const char **response_data, int *response_datalen) {
    const char *url = http_request_get_url(request);
    void *media_data_store = NULL;

    media_data_store = get_media_data_store(conn->raop);
    if (!media_data_store) {
        logger_log(conn->raop->logger, LOGGER_ERR, "media_data_store not found");
        return;
    }
    *response_datalen = query_media_data(media_data_store, url, response_data);
    http_response_add_header(response, "Access-Control-Allow-Headers", "Content-type");
    http_response_add_header(response, "Access-Control-Allow-Origin", "*");
    
    if (*response_datalen > 0) {
        http_response_add_header(response, "Content-Type", "application/x-mpegURL; charset=utf-8");
    } else if (*response_datalen == 0) {
      http_response_destroy(response);
      response = http_response_init("HTTP/1.1", 404, "Not Found");
    } 
 }

static void
http_handler_play(raop_conn_t *conn, http_request_t *request, http_response_t *response,
                      char **response_data, int *response_datalen) {
    const char* session_id = NULL;
    char* playback_location = NULL;
    plist_t req_root_node = NULL;
    double start_position = 0.0;
    float start_pos_in_ms = 0.0f;
    bool data_is_binary_plist = false;
    bool data_is_text = false;
    bool data_is_octet = false;
    void *media_data_store = NULL;
    bool ret;

    logger_log(conn->raop->logger, LOGGER_DEBUG, "http_handler_play");
    printf("airplay_video is at %p\n", conn->airplay_video);
    media_data_store = get_media_data_store(conn->raop);
    if (!media_data_store) {
      logger_log(conn->raop->logger, LOGGER_ERR, "media_data_store not found, conn = %p", conn);

        return;
    }
    printf("media_data_store is at %p\n", media_data_store);

    session_id = http_request_get_header(request, "X-Apple-Session-ID");
    if (!session_id) {
        logger_log(conn->raop->logger, LOGGER_ERR, "Play request had no X-Apple-Session-ID");
        goto play_error;
    }
    int request_datalen = -1;    
    const char *request_data = http_request_get_data(request, &request_datalen);
    printf("handler_play 2\n");    
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

#if 0	//this seems to have no purpose
        plist_t req_uuid_node = plist_dict_get_item(req_root_node, "uuid");
        if (!req_uuid_node) {
            goto play_error;    /* just check if uuid is present in plist, but make no use of it ? */
            // apsdk does store playback_uuid_
        } else {
            const char* playback_uuid;
            plist_get_string_val(req_content_location_node, &playback_uuid);
            set_playback_uuid(media_data_store, playback_uuid);
            free (playback_uuid);
	}
#endif
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
             plist_get_real_val(req_content_location_node, &start_position);
	     start_pos_in_ms = (float) (start_position * 1000);
	     set_start_pos_in_ms(media_data_store, start_pos_in_ms);
        }
    }
    printf("http handler play: request_media_data %s %s %p\n", playback_location, session_id, media_data_store);
    ret = request_media_data(media_data_store, playback_location, session_id);
    printf("return from request_media, ret = %d\n", ret);
    
    if (!ret) {
        /* normal play, not HLS: assume location is valid */
        logger_log(conn->raop->logger, LOGGER_INFO, "Play normal (non-HLS) video, location = %s", playback_location);
        airplay_video_play(conn->airplay_video, session_id, playback_location, get_start_pos_in_ms(media_data_store));
      }
    printf("handler_play: return from request_media_data \n");
    if (playback_location) {
        free (playback_location);
    }

    if (req_root_node) {
        plist_free(req_root_node);
    }
     return;

 play_error:;
    printf("play_error\n");
    if (req_root_node) {
      plist_free(req_root_node);
    }
    logger_log(conn->raop->logger, LOGGER_ERR, "Couldn't find valid Plist Data for /play, Unhandled");
    http_response_destroy(response);
    response = http_response_init("HTTP/1.1", 400, "Bad Request");
}


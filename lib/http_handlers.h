#include "airplay_video.h"

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
    int socket_fd = httpd_get_connection_socket (conn->raop->httpd, (void *) conn);
    if (socket_fd < 0) {
        logger_log(conn->raop->logger, LOGGER_ERR, "faied to retrieve socket_fd from httpd");
    }
    
    /* initialize the aiplay video service */
    const char *session_id = http_request_get_header(request, "X-Apple-Session-ID");

    conn->airplay_video =  (void *) airplay_video_service_init(conn->raop->logger, &(conn->raop->callbacks), conn,
                                                               conn->raop, socket_fd, conn->raop->port, session_id);

}

static void
http_handler_get_property(raop_conn_t *conn, http_request_t *request, http_response_t *response,
                          char **response_data, int *response_datalen) {
    const char *url = http_request_get_url(request);
    const char *property = strstr(url, "/setProperty?") + 1;
    logger_log(conn->raop->logger, LOGGER_DEBUG, "http_handler_get_property: %s (unhandled)", property);
}

static void
http_handler_reverse(raop_conn_t *conn, http_request_t *request, http_response_t *response,
                     char **response_data, int *response_datalen) {

    const char *purpose = http_request_get_header(request, "X-Apple-Purpose");
    const char *connection = http_request_get_header(request, "Connection");
    const char *upgrade = http_request_get_header(request, "Upgrade");
    logger_log(conn->raop->logger, LOGGER_INFO, "client requested reverse connection: %s; purpose: %s  \"%s\"",
               connection, upgrade, purpose);
    http_response_destroy(response);
    response = http_response_init("HTTP/1.1", 101, "Switching Protocols");
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

    const char *url = http_request_get_url(request);
    const char *data = strstr(url, "?");
    double rate_value = 0.0;
    if (data) {
        data++;
	const char *rate = strstr(data, "=") + 1;
	char *end;
	double value = strtod(rate, &end);
	if (end && end != rate) {
	  rate_value = value;
	  logger_log(conn->raop->logger, LOGGER_DEBUG, "http_handler_rate: got rate = %f", rate_value);	  
	}
    }

    const char *session_id = http_request_get_header(request, "X-Apple-Session-ID");
    airplay_video_rate(conn->airplay_video, session_id, rate_value);
}

static void
http_handler_stop(raop_conn_t *conn, http_request_t *request, http_response_t *response,
                  char **response_data, int *response_datalen) {
    logger_log(conn->raop->logger, LOGGER_INFO, "client HTTP request POST stop");
    airplay_media_reset(conn->airplay_video);
    const char *session_id = http_request_get_header(request, "X-Apple-Session-ID");
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
    int request_id = 0;
    int fcup_response_datalen = 0;
    int fcup_response_statuscode;

    plist_t plist_fcup_response_statuscode_node = plist_dict_get_item(req_params_node, "FCUP_Response_StatusCode");
    plist_get_uint_val(plist_fcup_response_statuscode_node, &uint_val);
    fcup_response_statuscode = (int) uint_val;
    uint_val = 0;
    if (!fcup_response_statuscode) {
        logger_log(conn->raop->logger, LOGGER_INFO, "unhandledURLResponse with non-zero FCUP_Response_StatusCode = %d",
                   fcup_response_statuscode);
        goto post_action_error;
    }

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

    plist_t plist_fcup_response_requestid_node = plist_dict_get_item(req_params_node, "FCUP_Response_RequestID");    
    plist_get_uint_val(plist_fcup_response_requestid_node, &uint_val);
    request_id = (int) uint_val;
    
    char *location = airplay_process_media_data(conn->airplay_video, fcup_response_url, fcup_response_data,
                                                fcup_response_datalen, request_id); 
    
    /* play, if location != NULL */
    if (location) {
      const char *session_id = http_request_get_header(request, "X-Apple-Session-ID");
      double position_in_secs = 0.0;
      airplay_video_play(conn->airplay_video, session_id, location, position_in_secs);
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
http_handler_get_generic(raop_conn_t *conn,
                      http_request_t *request, http_response_t *response,
			  char **response_data, int *response_datalen) {
    const char *url = http_request_get_url(request);
    *response_datalen  =  query_media_data(conn->airplay_video, url, response_data);
    http_response_add_header(response, "Content-Type", "application/x-mpegURL; charset=utf-8");
    logger_log(conn->raop->logger, LOGGER_ERR, "http_handler_get_generic is incomplete");
    assert(0);   
}

static void
http_handler_set_property(raop_conn_t *conn,
                      http_request_t *request, http_response_t *response,
			  char **response_data, int *response_datalen) {

    const char *url = http_request_get_url(request);
    const char *property = strstr(url, "/setProperty?") + 1;
    logger_log(conn->raop->logger, LOGGER_DEBUG, "http_handler_set_property: %s", property);

    if (!strcmp(url, "reverseEndTime") ||
        !strcmp(url, "forwardEndTime") ||
        !strcmp(url, "actionAtItemEnd")) {
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
http_handler_play(raop_conn_t *conn,
                      http_request_t *request, http_response_t *response,
                      char **response_data, int *response_datalen) {
    const char* session_id = NULL;
    char* playback_location = NULL;
    plist_t req_root_node = NULL;
    double start_position = 0.0;
    bool data_is_binary_plist = false;
    bool data_is_text = false;
    bool data_is_octet = false;
    logger_log(conn->raop->logger, LOGGER_DEBUG, "http_handler_play");
    
    session_id = http_request_get_header(request, "X-Apple-Session-ID");
    if (!session_id) {
        logger_log(conn->raop->logger, LOGGER_ERR, "Play request had no X-Apple-Session-ID");
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

#if 0	//this seems to have no purpose
        plist_t req_uuid_node = plist_dict_get_item(req_root_node, "uuid");
        if (!req_uuid_node) {
	  goto play_error;    /* just check if uuid is present in plist, but make no use of it ? */
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
        }
    }

    airplay_video_play(conn->airplay_video, session_id, playback_location, start_position);
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

/* File that Handles Information about Casting
GenghisKhanDrip*/

#include "casting.h"
#include <regex.h>
#include <stdio.h>
#include "utils.h"
#include <plist/plist.h>

bool isHLSUrl(char* url) {
    regex_t checkHTTP;
    regex_t checkM3U8;

    int result;
    result = regcomp(&checkHTTP, "^(https?://)", REG_EXTENDED);
    result = regcomp(&checkM3U8, "\\.m3u8$", REG_EXTENDED);
    //TODO: Handle Errors
    
    result = regexec(&checkHTTP, url, 0, NULL, 0);

    if (result == 0) {
        result = regexec(&checkM3U8, url, 0, NULL, 0);
        if (result == 0) {
            return true;
        } else if (result == REG_NOMATCH) {
            return false;
        } else {
            return false;
        }
    } else if (result == REG_NOMATCH) {
        return true;
    } else {
        //TODO: Handle Errors
        return false;
    }
}

void startHLSRequests(hls_cast_t *cast)
{

    const char* found = strstr(cast->playback_location, "://");

    if (found) {
        // Calculate the length up to the delimiter
        size_t length = found - cast->playback_location;

        // Extract the substring
        char protocol[length];
        strncpy(protocol, cast->playback_location, length);

        printf("Result: %s\n", protocol);

        char* hostname1 = "localhost:";
        char* hostname = malloc(strlen(hostname1) + strlen(cast->port));
        strcpy(hostname, hostname1);
        concatenate_string(hostname, cast->port);
        str_replace(cast->playback_location, protocol, "http");
        str_replace(cast->playback_location, "127.0.0.1", hostname);
        str_replace(cast->playback_location, "localhost", hostname);
        printf("String is %s\n", cast->playback_location);

        plist_t fcup_req = plist_new_dict();

        plist_t session = plist_new_uint(1);
        plist_dict_set_item(fcup_req, "sessionID", session);

        plist_t type = plist_new_string("unhandledURLRequest");
        plist_dict_set_item(fcup_req, "type", type);

        plist_t fcup_data = plist_new_dict();
        plist_t clientinfo = plist_new_uint(1);
        plist_dict_set_item(fcup_data, "FCUP_Response_ClientInfo", clientinfo);
        plist_t clientref = plist_new_uint(40030004);
        plist_dict_set_item(fcup_data, "FCUP_Response_ClientRef", clientref);
        plist_t reqid = plist_new_uint(cast->requestid++);
        plist_dict_set_item(fcup_data, "FCUP_Response_RequestID", reqid);
        plist_t url = plist_new_string(cast->playback_location);
        plist_dict_set_item(fcup_data, "FCUP_Response_URL", url);

        plist_t plheaders = plist_new_dict();
        plist_t plsessionid = plist_new_string(cast->cast_session);
        plist_dict_set_item(plheaders, "X-Playback-Session-Id", plsessionid);
        plist_t agent = plist_new_string("AppleCoreMedia/1.0.0.11B554a (Apple TV; U; CPU OS 7_0_4 like Mac OS X; en_us)");
        plist_dict_set_item(plheaders, "User-Agent", agent);

        plist_dict_set_item(fcup_data, "FCUP_Response_Headers", plheaders);
        plist_dict_set_item(fcup_req, "request", fcup_data);

        char *xml = NULL;
        uint32_t plength = 0;
        plist_to_xml(fcup_req, &xml, &plength);
        plength--;
        printf("%s", xml);
        plist_free(fcup_req);
    } else {
        printf("Delimiter not found in the input.\n");
    }

}

/* File that Handles Information about Casting
GenghisKhanDrip*/

#include "casting.h"
#include <regex.h>

bool isHLSUrl(char* url) {
    regex_t checkHTTP;
    regex_t checkM3U8;

    int result;
    result = regcomp(&checkHTTP, "^(https?://)", REG_EXTENDED);
    result = regcomp(&checkM3U8, "\\.m3u8$", REG_EXTENDED);
    //TODO: Handle Errors
    
    result = regexec(&checkHTTP, url, 0, NULL, 0);

    if (result == 0) {
        result = regexec(&checkHTTP, url, 0, NULL, 0);
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

    regex_t checkHTTP;
    regex_t checkM3U8;
    regmatch_t matches[2];

    int result;
    result = regcomp(&checkHTTP, "^(https?://)", REG_EXTENDED);
    result = regcomp(&checkM3U8, "\\.m3u8$", REG_EXTENDED);
    //TODO: Handle Errors
    
    result = regexec(&checkHTTP, cast->playback_location, 2, matches, 0);

    if (result == 0) {
        result = regexec(&checkHTTP, cast->playback_location, 0, NULL, 0);
        if (result == 0) {
            return;
        } else if (result == REG_NOMATCH) {
            return;
        } else {
            return;
        }
    } else if (result == REG_NOMATCH) {
        return;
    } else {
        //TODO: Handle Errors
        return;
    }

}

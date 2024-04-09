/* File that handles information regarding casting data
 GenghisKhanDrip*/

#ifndef CASTING
#define CASTING

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

struct hls_cast_s {
    const char *cast_session;
    int castsessionlen;

    char* playback_uuid;
    char* playback_location;
};
typedef struct hls_cast_s hls_cast_t;

bool isHLSUrl(char* url);
void startHLSRequests(hls_cast_t* cast);

#endif
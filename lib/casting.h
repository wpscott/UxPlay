#ifndef CASTING
#define CASTING



struct casting_data_s {
    char *session_id;
    char *uuid;
    char *location;
    char *scheme;
    float start_pos_ms;
} typedef casting_data_t;


void casting_data_destroy(casting_data_t *casting_data);


#endif

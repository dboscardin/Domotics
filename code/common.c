#include <stdio.h>
#include <stdlib.h>

Response parse_response(const char *raw) {
    Response r = {0};
    char *space = strchr(raw, ' ');
    if (space) {
        *space = '\0';
        r.code = atoi(raw);
        strncpy(r.payload, space + 1, MAX_MSG_LEN - 1);
    } else {
        r.code = atoi(raw);
        r.payload[0] = '\0';
    }
    return r;
}

const char *get_field(const char *payload, const char *key);
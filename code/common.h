//forse da rimuovere
#ifndef COMMON_H
#define COMMON_H

#define SUCCESS 0
#define ERROR_GENERIC 1

typedef struct {
    int code;
    char payload[MAX_MSG_LEN];
} Response;

#endif
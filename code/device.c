#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <sys/types.h>
#include "device.h"
#include "protocol.h"

void handle_set_parent(int *parent_id, const char *buffer) {
    int new_parent_id;
    if (sscanf(buffer, "%*s %d", &new_parent_id) == 1) {
        *parent_id = new_parent_id;
    }
}

void format_parent_string(int parent_id, char *parent_str, size_t size) {
    if (parent_id == -1) {
        snprintf(parent_str, size, "None");
    } else if (parent_id == CONTROLLER_ID) {
        snprintf(parent_str, size, "Controller");
    } else {
        snprintf(parent_str, size, "Hub/Timer ID %d", parent_id);
    }
}

long compute_elapsed_seconds(struct timespec *active_since) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (long)(now.tv_sec - active_since->tv_sec);
}
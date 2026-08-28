#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <sys/types.h>
#include "device.h"
#include "protocol.h"

// Extracts the new parent_id from the IPC command and updates the device variable
void handle_set_parent(int *parent_id, const char *buffer) {
    int new_parent_id;
    // Reads the buffer skipping the first word
    if (sscanf(buffer, "%*s %d", &new_parent_id) == 1) {
        *parent_id = new_parent_id;
    }
}

// Formats the parent_id into a readable string
void format_parent_string(int parent_id, char *parent_str, size_t size) {
    if (parent_id == -1) {
        snprintf(parent_str, size, "None");
    } else if (parent_id == CONTROLLER_ID) {
        snprintf(parent_str, size, "Controller");
    } else {
        snprintf(parent_str, size, "Hub/Timer ID %d", parent_id);
    }
}

// Computes elapsed seconds from a given timestamp
long compute_elapsed_seconds(struct timespec *active_since) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (long)(now.tv_sec - active_since->tv_sec);
}
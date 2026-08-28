// Common declarations for all devices
#ifndef DEVICE_H
#define DEVICE_H

#include <sys/types.h>
#include <stdbool.h>

// Device type
typedef enum {
    DEVICE_CONTROLLER,
    DEVICE_HUB,
    DEVICE_TIMER,
    DEVICE_BULB,
    DEVICE_WINDOW,
    DEVICE_FRIDGE
} DeviceType;

typedef struct {
    int id;
    pid_t pid;
    DeviceType type;
    int fifo_fd;
    int parent_id;
} DeviceInfo;

// For hub and timer
typedef struct {
    int id;
    DeviceType type;
} ChildDevice;

// Extracts the new parent_id from the IPC command and updates the device variable
void handle_set_parent(int *parent_id, const char *buffer);

// Formats the parent_id into a readable string
void format_parent_string(int parent_id, char *parent, size_t size);

// Computes elapsed seconds from a given timestamp
long compute_elapsed_seconds(struct timespec *active_since);

#endif
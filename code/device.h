//qui solo ciò che è comune a tutti i device
#ifndef DEVICE_H
#define DEVICE_H

#include <sys/types.h>
#include <stdbool.h>

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
    int parent_id; // -1 se non è collegato a nulla
} DeviceInfo;

#endif
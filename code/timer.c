#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>

#include "timer.h"
#include "ipc.h"
#include "device.h"

#define BUFFER_SIZE 256

static const char *get_device_type_name(DeviceType type) {
    switch (type) {
        case DEVICE_BULB:       return "Bulb";
        case DEVICE_WINDOW:     return "Window";
        case DEVICE_FRIDGE:     return "Fridge";
        case DEVICE_CONTROLLER: return "Controller";
        case DEVICE_HUB:        return "Hub";
        case DEVICE_TIMER:      return "Timer";
        default:                return "Unknown";
    }
}

void timer_init(TimerDevice *timer, int id) {
    timer->id = id;
    timer->parent_id = -1;
    timer->num_children = 0;
    timer->timer_delay = 0;
    timer->time_left = 0;
    timer->is_active = false;
}

void create_timer(int id) {
    TimerDevice timer;
    timer_init(&timer, id);

    exit(0);
}
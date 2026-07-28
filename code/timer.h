#ifndef TIMER_H
#define TIMER_H

#include <stdbool.h>
#include "device.h"

typedef struct {
    int id;
    int target_id;    
    DeviceType target_type;
    int time_seconds;
    bool is_active;
} TimerDevice;

void create_timer(int id);

#endif
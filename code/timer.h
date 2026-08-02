#ifndef TIMER_H
#define TIMER_H

#include <stdbool.h>
#include "device.h"

#define MAX_CHILDREN 50

typedef struct {
    int id;
    int parent_id;
    ChildDevice children[MAX_CHILDREN];
    int num_children;
    int timer_delay;
    int time_left;
    bool is_active;
} TimerDevice;


void timer_init(TimerDevice *timer, int id);
void create_timer(int id);


#endif
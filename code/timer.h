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
    bool target_state;
} TimerDevice;


void timer_init(TimerDevice *timer, int id);
bool timer_add_child(TimerDevice *timer, int child_id,DeviceType child_type);
bool timer_remove_child(TimerDevice *timer, int child_id);
void create_timer(int id);



#endif
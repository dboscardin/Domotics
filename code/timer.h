#ifndef TIMER_H
#define TIMER_H

#include <stdbool.h>
#include "device.h"

// il timer può avere solo un figlio
#define MAX_TIMER_CHILDREN 1

typedef struct {
    int id;
    int parent_id;
    ChildDevice children[MAX_TIMER_CHILDREN];
    int num_children;
    char begin[6];  //HH:MM
    char end[6];    //HH:MM
} TimerDevice;


TimerDevice create_timer_struct(int id);
void timer_run(TimerDevice *timer);
void create_timer(int id);



#endif
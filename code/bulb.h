#ifndef BULB_H
#define BULB_H

#include <stdbool.h>

typedef struct {
    int id;
    int parent_id;
    bool power;                     // Bulb power state
    long time;                      // Total accumulated power-on time
    struct timespec active_since;   // Timestamp of when the active state began
    bool tracking;                  // Flag indicating if elapsed time is currently tracked
} Bulb;

Bulb create_bulb_struct(int id);

void bulb_run(Bulb *bulb);

void create_bulb(int id);

#endif
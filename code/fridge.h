#ifndef FRIDGE_H
#define FRIDGE_H

#include <stdbool.h>

typedef struct {
    int id;
    int parent_id;
    bool is_open;                   // Door state
    long time;                      // Total accumulated open time
    int delay;                      // Delay after which the fridge auto-closes
    int perc;                       // Fill percentage
    int temp;                       // Current internal temperature
    int thermostat;                 // Target thermostat temperature
    struct timespec active_since;   // Timestamp of the last opening for tracking
    bool tracking;                  // true if door is currently open
} Fridge;

Fridge create_fridge_struct(int id);

void fridge_run(Fridge *fridge);

void create_fridge(int id);

#endif
#ifndef FRIDGE_H
#define FRIDGE_H

#include <stdbool.h>

typedef struct {
    int id;
    int parent_id;
    bool is_open;
    long time;
    int delay;
    int perc;
    int temp;
    int thermostat;
    struct timespec active_since;   // timestamp di quando è iniziato lo stato attivo
    bool tracking;                  // true se il timer è attivo
} Fridge;

Fridge create_fridge_struct(int id);

void fridge_run(Fridge *fridge);

void create_fridge(int id);

#endif
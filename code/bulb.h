#ifndef BULB_H
#define BULB_H

#include <stdbool.h>

typedef struct {
    int id;
    int parent_id;
    bool power;                     // stato della lampadina
    long time;                      // tempo totale di accensione accumulato
    struct timespec active_since;   // timestamp di quando è iniziato lo stato attivo
    bool tracking;                  // flag per indicare se stiamo attualmente contando il tempo

} Bulb;

Bulb create_bulb_struct(int id);

void bulb_run(Bulb *bulb);

void create_bulb(int id);

#endif
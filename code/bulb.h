#ifndef BULB_H
#define BULB_H

#include <stdbool.h>

typedef struct {
    int id;
    int parent_id;
    bool power;
    long time;
    struct timespec active_since;   // timestamp di quando è iniziato lo stato attivo
    bool tracking;                  // true se il timer è attivo

} Bulb;

Bulb create_bulb_struct(int id);

void bulb_run(Bulb *bulb);

void create_bulb(int id);

#endif
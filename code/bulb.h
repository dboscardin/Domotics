#ifndef BULB_H
#define BULB_H

#include <stdbool.h>

typedef struct {
    int id;
    bool power;
    int time;
} Bulb;

Bulb create_bulb_struct(int id);

void bulb_run(Bulb *bulb);

void create_bulb(int id);

#endif
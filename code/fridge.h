#ifndef FRIDGE_H
#define FRIDGE_H

#include <stdbool.h>

typedef struct {
    int id;
    bool is_open;
    int time;
    int delay;
    int perc;
    int temp;
    int thermostat;
} Fridge;

Fridge create_fridge_struct(int id);

void fridge_run(Fridge *fridge);

void create_fridge(int id);

#endif
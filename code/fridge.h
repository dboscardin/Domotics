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

void create_fridge(int id);

#endif
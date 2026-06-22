#ifndef FRIDGE_H
#define FRIDGE_H

#include <stdbool.h>

typedef struct {
    int id;
    bool power;
    int time;
    int delay;
    int perc;
    int temp;
    int thermostat;
} Fridge;

void create_Fridge(int id);

#endif
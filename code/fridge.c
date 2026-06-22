#include <stdio.h>
#include <unistd.h>
#include "fridge.h"

void create_fridge(int id) {
    Fridge fridge = {
        .id = id,
        .is_open = false,
        .time = 0,
        .delay = 60, //60s
        .perc = 100,
        .temp = 6,
        .thermostat = 6 
    };
}
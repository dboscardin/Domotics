#include <stdio.h>
#include <unistd.h>
#include "bulb.h"

void create_bulb(int id) {
    Bulb bulb = {
        .id = id,
        .power = false,
        .timer = 0
    };
}
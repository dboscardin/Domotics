#include <stdio.h>
#include <unistd.h>
#include "fridge.h"

#define BUFFER_SIZE 50

Fridge create_fridge(int id) {
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

void fridge_run(Fridge *fridge) {
    ipc_create_fifo(fridge->id, 5);
    int fd = ipc_open_for_listening(fridge->id, 5);
    char buffer[BUFFER_SIZE];
    while(1) {
        ipc_read_line(fd, buffer, sizeof(buffer));
    }
}
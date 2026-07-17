#include <stdio.h>
#include <unistd.h>
#include "bulb.h"
#include "ipc.h"

#define BUFFER_SIZE 50

Bulb create_bulb(int id) {
    Bulb bulb = {
        .id = id,
        .power = false,
        .time = 0
    };

    return bulb;
}

void bulb_run(Bulb *bulb) {
    ipc_create_fifo(bulb->id, 3);
    int fd = ipc_open_for_listening(bulb->id, 3);
    char buffer[BUFFER_SIZE];
    while(1) {
        ipc_read_line(fd, buffer, sizeof(buffer));
    }
}
#include <stdio.h>
#include <unistd.h>
#include "bulb.h"
#include "ipc.h"

#define BUFFER_SIZE 50

Bulb create_bulb_struct(int id) {
    Bulb bulb = {
        .id = id,
        .power = false,
        .time = 0
    };

    return bulb;
}

void bulb_run(Bulb *bulb) {
    int fd = ipc_open_for_listening(bulb->id, DEVICE_BULB);
    char buffer[BUFFER_SIZE];
    while(1) {
        ipc_read_line(fd, buffer, sizeof(buffer));
    }
    close(fd);
}

void create_bulb(int id) {
    Bulb b = create_bulb_struct(id);
    bulb_run(&b);
}
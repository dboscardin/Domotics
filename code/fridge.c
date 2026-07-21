#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include "fridge.h"
#include "ipc.h"

#define BUFFER_SIZE 50

Fridge create_fridge_struct(int id) {
    Fridge fridge = {
        .id = id,
        .is_open = false,
        .time = 0,
        .delay = 60, //60s
        .perc = 100,
        .temp = 6,
        .thermostat = 6 
    };
    return fridge;
}

void fridge_run(Fridge *fridge) {
    int fd = ipc_open_for_listening(fridge->id, DEVICE_FRIDGE);
    char buffer[BUFFER_SIZE];
    while(1) {
        int bytes = ipc_read_line(fd, buffer, sizeof(buffer));
        if (bytes > 0) {
        
            printf("Message recevied: '%s'\n",buffer);

            //delete
            if (strncmp(buffer, "DELETE",6) == 0){
                printf("Closed Fridge ID:%d\n", fridge->id);
                close(fd);
                exit(0);
            }
        } else {
            usleep(50000); // il processo consuma meno risorse
        }
    }
    close(fd);
}

void create_fridge(int id) {
    Fridge f = create_fridge_struct(id);
    fridge_run(&f);
}
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "bulb.h"
#include "ipc.h"

#define BUFFER_SIZE 50
#define _DEFAULT_SOURCE

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

    srand(time(NULL));

    while(1) {
        int bytes = ipc_read_line(fd, buffer, sizeof(buffer));
        if (bytes > 0) {
            sleep((rand() % 3) + 1);
        
            printf("Message recevied: '%s'\n",buffer);

            //delete
            if (strncmp(buffer, "DELETE", 6) == 0){
                printf("Closed Bulb ID:%d\n", bulb->id);  
                close(fd);
                exit(0);
            }
            else if(strncmp(buffer, "SWITCH", 6) == 0) {
                char label[32], pos[32];
                sscanf(buffer, "SWITCH %s %s", label, pos);

                if(strcmp(label, "power") == 0) {
                    bulb->power = (strcmp(pos, "on") == 0);
                }
                printf("[Bulb %d] Power set to: %s\n", bulb->id, bulb->power ? "ON" : "OFF");
            }
        } else {
            usleep(50000); // il processo consuma meno risorse
        }
    }
    close(fd);
}

void create_bulb(int id) {
    Bulb b = create_bulb_struct(id);
    bulb_run(&b);
}
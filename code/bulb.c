#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "bulb.h"
#include "ipc.h"
#include "protocol.h"

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

    srand(time(NULL) ^ getpid());

    int fd = ipc_open_for_listening(bulb->id, DEVICE_BULB);
    char buffer[BUFFER_SIZE];


    while(1) {
        int bytes = ipc_read_line(fd, buffer, sizeof(buffer));
        if (bytes > 0) {
            //delay
            ipc_simulate_delay();

            //delete
            if (strncmp(buffer, CMD_DELETE, strlen(CMD_DELETE)) == 0){
                ipc_send_controller(STATUS_OK, "Device deleted.");
                close(fd);
                exit(0);
            }
            //switch
            else if(strncmp(buffer, CMD_SWITCH, strlen(CMD_SWITCH)) == 0) {
                char label[32], pos[32];
                sscanf(buffer, "%*s %s %s", label, pos);

                if(strcmp(label, "power") == 0) {
                    if (strcmp(pos, "on") == 0) {
                        bulb->power = true;
                    } else if (strcmp(pos, "off") == 0) {
                        bulb->power = false;
                    } else {
                        ipc_send_controller(ERR_INVALID_PARAM, "Invalid position for power.");
                        continue;
                    }

                    // Invia il messaggio al controller
                    char message[MAX_MSG_LEN];
                    snprintf(message, sizeof(message), "Bulb %d, Power set to: %s", bulb->id, bulb->power ? "ON" : "OFF");
                    ipc_send_controller(STATUS_OK, message);
                } else {
                    ipc_send_controller(ERR_INVALID_PARAM, "Invalid label for Bulb.");
                }
            }
            else if(strncmp(buffer , CMD_INFO, strlen(CMD_INFO)) == 0){
                char message[MAX_MSG_LEN];

                snprintf(message,sizeof(message),
                "\n-------- Bulb Details ------\n"
                "ID: %d\n"
                "State: %s\n"
                "Total usage time: %d seconds\n"
                "----------------------------\n", 
                bulb->id, bulb->power ? "On" : "Off", bulb->time);
                ipc_send_controller(STATUS_OK, message);
            }else {
                ipc_send_controller(ERR_INVALID_COMMAND, "Unkown command.");
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
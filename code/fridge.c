#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "fridge.h"
#include "ipc.h"
#include "device.h"

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

    srand(time(NULL) ^ getpid());

    int fd = ipc_open_for_listening(fridge->id, DEVICE_FRIDGE);
    char buffer[BUFFER_SIZE];
    while(1) {
        int bytes = ipc_read_line(fd, buffer, sizeof(buffer));
        if (bytes > 0) {

            //delay
            ipc_simulate_delay();
        
            printf("Message recevied: '%s'\n",buffer);

            //delete
            if (strncmp(buffer, "DELETE",6) == 0){
                printf("Closed Fridge ID:%d\n", fridge->id);
                close(fd);
                exit(0);
            }
            else if(strncmp(buffer, "SWITCH", 6) == 0) {
                char label[32], pos[32];
                sscanf(buffer, "SWITCH %s %s", label, pos);

                if(strcmp(label, "is_open") == 0) {
                    fridge->is_open = (strcmp(pos, "on") == 0);
                }
                if(strcmp(label, "time") == 0) {
                    fridge->time = atoi(pos);
                }
                else if(strcmp(label, "delay") == 0) {
                    fridge->delay = atoi(pos);
                }
                //in seguito far sì che perc e therm siano modificati solo manualmente
                else if(strcmp(label, "perc") == 0) {
                    fridge->perc = atoi(pos);
                }
                else if(strcmp(label, "temp") == 0) {
                    fridge->temp = atoi(pos);
                }
                else if(strcmp(label, "thermostat") == 0) {
                    fridge->thermostat = atoi(pos);
                }
                printf("[Fridge %d] %s set to: %s\n", fridge->id, label, pos);
                fflush(stdout);
            }
            else if(strncmp(buffer , "INFO", 4) == 0){
                printf("------- Fridge Details -----\n");
                printf("ID: %d\n", fridge->id);
                printf("Door State: %s\n", fridge->is_open ? "Open" : "Closed");
                printf("Time left open: %d s\n", fridge->time);
                printf("Delay: %d s\n", fridge->delay);
                printf("Fill percentage: %d%%\n", fridge->perc);
                printf("Current Temp: %d °C\n", fridge->temp);
                printf("Thermostat: %d °C\n", fridge->thermostat);
                printf("----------------------------\n\n");
                fflush(stdout);
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
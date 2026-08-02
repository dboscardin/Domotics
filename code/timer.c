#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>

#include "timer.h"
#include "ipc.h"
#include "device.h"

#define BUFFER_SIZE 256

static const char *get_device_type_name(DeviceType type) {
    switch (type) {
        case DEVICE_BULB:       return "Bulb";
        case DEVICE_WINDOW:     return "Window";
        case DEVICE_FRIDGE:     return "Fridge";
        case DEVICE_CONTROLLER: return "Controller";
        case DEVICE_HUB:        return "Hub";
        case DEVICE_TIMER:      return "Timer";
        default:                return "Unknown";
    }
}

void timer_init(TimerDevice *timer, int id) {
    timer->id = id;
    timer->parent_id = -1;
    timer->num_children = 0;
    timer->timer_delay = 0;
    timer->time_left = 0;
    timer->is_active = false;
}

void create_timer(int id) {
    TimerDevice timer;
    timer_init(&timer, id);

    int fd_ascolto = ipc_open_for_listening(timer.id, DEVICE_TIMER);
    if (fd_ascolto == -1){
        fprintf(stderr,"Error: unable to open listening FIFO for Timer %d\n", id);
        exit(1);
    }

    char buffer[BUFFER_SIZE];
    int ticks = 0;
    while(1){
        int bytes_letti = ipc_read_line(fd_ascolto, buffer, sizeof(buffer));

        if(bytes_letti > 0){
            printf("Message received: '%s'\n", buffer);
        

            //INFO
            if(strncmp(buffer, "INFO", 4) == 0){
                    printf("-------- Timer Details -----\n");
                    printf("ID: %d\n", timer.id);
                    if (timer.parent_id == -1) {
                        printf("Linked to Hub: NO\n");
                    } else {
                        printf("Linked to Hub ID: %d\n", timer.parent_id);
                    }
                    printf("Delay set: %d s\n", timer.timer_delay);
                    printf("Status: %s\n", timer.is_active ? "Running" : "Stopped");
                    if (timer.is_active) {
                        printf("Time remaining: %d s\n", timer.time_left);
                    }
                    printf("Connected devices count: %d\n", timer.num_children);
                    if (timer.num_children > 0) {
                        printf("Linked Devices:\n");
                        for (int i = 0; i < timer.num_children; i++) {
                            printf("  %d) ID: %d | Type: %s\n", 
                                i + 1, 
                                timer.children[i].id, 
                                get_device_type_name(timer.children[i].type));
                        }
                    }
                    printf("----------------------------\n\n");
                    fflush(stdout);


            }

        } else {
            usleep(50000);
        }
    }

    close(fd_ascolto);
    exit(0);
}
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

bool timer_add_child(TimerDevice *timer, int child_id,DeviceType child_type){
    if(timer->num_children >= MAX_CHILDREN){
        fprintf(stderr, "Error: max children limit reached for Timer %d\n", timer->id);
        return false;
    }

    for(int i=0; i<timer->num_children; i++){
        if(timer->children[i].id == child_id){
            printf("Child ID %d is already linked to Timer %d.\n", child_id, timer->id);
            return true;
        }
    }

    timer->children[timer->num_children].id = child_id;
    timer->children[timer->num_children].type = child_type;
    timer->num_children++;

    printf("Timer %d linked child ID: %d (%s)\n", timer->id, child_id, get_device_type_name(child_type));
    fflush(stdout);
    return true;

}

bool timer_remove_child(TimerDevice *timer, int child_id) {
    int index = -1;
    for (int i = 0; i < timer->num_children; i++) {
        if (timer->children[i].id == child_id) {
            index = i;
            break;
        }
    }

    if (index == -1) return false;

    for (int i = index; i < timer->num_children - 1; i++) {
        timer->children[i] = timer->children[i + 1];
    }
    timer->num_children--;

    printf("Timer %d unlinked child ID: %d\n", timer->id, child_id);
    fflush(stdout);
    return true;
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
            
            //LINK
            if(strncmp(buffer, "LINK_CHILD", 10) == 0){
                int child_id, child_type_int;
                if (sscanf(buffer, "LINK_CHILD %d %d", &child_id, &child_type_int) == 2) {
                    timer_add_child(&timer, child_id, (DeviceType)child_type_int);
                }
            }
            
            // UNLINK
            else if (strncmp(buffer, "UNLINK_CHILD", 12) == 0) {
                int child_id;
                if (sscanf(buffer, "UNLINK_CHILD %d", &child_id) == 1) {
                    timer_remove_child(&timer, child_id);
                }
            }


            //INFO
            else if(strncmp(buffer, "INFO", 4) == 0){
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

            //DELETE
            else if(strncmp(buffer, "DELETE", 6) == 0){
                printf("Deleted Timer ID: %d\n ", timer.id);
                printf("Timer ID: %d closed successfully.\n", timer.id);
                fflush(stdout);
                close(fd_ascolto);
                exit(0);
            }

        } else {
            usleep(50000);
        }
    }

    close(fd_ascolto);
    exit(0);
}
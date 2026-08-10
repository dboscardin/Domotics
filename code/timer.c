#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <time.h>

#include "timer.h"
#include "ipc.h"
#include "device.h"
#include "protocol.h"

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

TimerDevice create_timer_struct(int id){

    TimerDevice timer = {
        .id = id,
        .parent_id = -1,
        .num_children = 0,
        .begin = "",
        .end = ""
    };
    return timer;
}


void timer_run(TimerDevice *timer){

    srand(time(NULL) ^ getpid());

    int fd_ascolto = ipc_open_for_listening(timer->id,DEVICE_TIMER);
    char buffer[BUFFER_SIZE];

    while(1){

        int bytes_letti = ipc_read_line(fd_ascolto,buffer,sizeof(buffer));

        if(bytes_letti > 0){

            ipc_simulate_delay();

            //link
            if(strncmp(buffer, CMD_LINK_CHILD, strlen(CMD_LINK_CHILD)) == 0){
                int child_id;
                int child_type;
                
                if(sscanf(buffer,"%*s %d %d ", &child_id, &child_type) == 2){
                    if(timer->num_children >= MAX_TIMER_CHILDREN){
                        ipc_send_controller(ERR_LINK_FAILED, "Failed to link: Timer can only have 1 direct child.");
                    } else {
                        timer->children[0].id = child_id;
                        timer->children[0].type = child_type;
                        timer->num_children = 1;
                        char msg[MAX_MSG_LEN];
                        snprintf(msg,sizeof(msg),"Link completed: Device %d is now scheduled by Timer %d.", child_id, timer->id);
                        ipc_send_controller(STATUS_OK,msg);
                    }
                }
            }
            //unlink
            else if(strncmp(buffer, CMD_UNLINK_CHILD, strlen(CMD_UNLINK_CHILD)) == 0){
                
                int child_id;
                if (sscanf(buffer, "%*s %d", &child_id) == 1){
                    if (timer->num_children > 0 && timer->children[0].id == child_id) {
                        timer->num_children = 0;
                        char msg[MAX_MSG_LEN];
                        snprintf(msg,sizeof(msg), "Unlink completed: Device %d removed from Timer.", child_id);
                        ipc_send_controller(STATUS_OK, msg);
                    } else {
                        char msg[MAX_MSG_LEN];
                        snprintf(msg,sizeof(msg), "Notice: Device %d is not linked to this Timer.", child_id);
                        ipc_send_controller(ERR_NOT_FOUND, msg);
                    }
                }
            }
            //switch
            else if(strncmp(buffer, CMD_SWITCH, strlen(CMD_SWITCH)) == 0){


            }
            //delete
            else if(strncmp(buffer, CMD_DELETE, strlen(CMD_DELETE)) == 0){

                for(int i=0; i<timer->num_children; i++){
                    int child_id = timer->children[i].id;
                    DeviceType child_type = timer->children[i].type;

                    int fd_child = ipc_open_for_writing(child_id,child_type);
                    if(fd_child != -1){
                        ipc_send_message(fd_child, CMD_DELETE);
                        close(fd_child);
                    }

                }

                char msg[MAX_MSG_LEN];
                snprintf(msg, sizeof(msg), "Timer %d and all its children deleted.", timer->id);
                ipc_send_controller(STATUS_OK, msg);
                close(fd_ascolto);
                exit(0);

            }
            //info
            else if(strncmp(buffer, CMD_INFO, strlen(CMD_INFO)) == 0){
                char message[MAX_MSG_LEN];
                int offset = 0;

                offset += snprintf(message + offset, sizeof(message) - offset,
                "\n------- Timer Details ------\n"
                "ID: %d\n"
                "Schedule: %s -> %s\n", 
                timer->id, 
                strlen(timer->begin) > 0 ? timer->begin : "Not set",
                strlen(timer->end) > 0 ? timer->end : "Not set");

                if (timer->num_children == 0) {
                    offset += snprintf(message + offset, sizeof(message) - offset, "Linked Device: NONE\n");
                } else {
                    // chiedo lo stato al figlio
                    int fd_child = ipc_open_for_writing(timer->children[0].id, timer->children[0].type);
                    if (fd_child != -1) {
                        char mirror_cmd[32];
                        snprintf(mirror_cmd, sizeof(mirror_cmd), "%s %d", CMD_MIRROR, timer->id);
                        ipc_send_message(fd_child, mirror_cmd);
                        close(fd_child);
                    }

                    char child_state[32] = "Unknown";
                    char mirror_buf[512];
                    
                    // Attendiamo la risposta
                    int max_retries = 20; 
                    while (max_retries > 0) {
                        if (ipc_read_line(fd_ascolto, mirror_buf, sizeof(mirror_buf)) > 0) {
                            if (strstr(mirror_buf, CMD_MIRROR_RESP) != NULL) {
                                sscanf(mirror_buf, "%*s %*d %31s", child_state);
                                break;
                            }
                        }
                        usleep(50000);
                        max_retries--;
                    }

                    offset += snprintf(message + offset, sizeof(message) - offset, 
                        "Linked Device ID: %d | Type: %s | State: %s\n", 
                        timer->children[0].id, get_device_type_name(timer->children[0].type), child_state);
                }
                
                snprintf(message + offset, sizeof(message) - offset, "----------------------------\n");
                ipc_send_controller(STATUS_OK, message);

            } else {
                ipc_send_controller(ERR_INVALID_COMMAND,"Unkown command.");
            }
        } else{
            usleep(50000);
        }
    }

    close(fd_ascolto);
    exit(0);
}



void create_timer(int id){
    TimerDevice timer = create_timer_struct(id);
    timer_run(&timer);
}
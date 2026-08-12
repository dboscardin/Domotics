#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include "bulb.h"
#include "ipc.h"
#include "protocol.h"
#include "device.h"

#define BUFFER_SIZE 256

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

                int sender = -1;
                int sender_type = -1;
                
                if(sscanf(buffer, "%*s %d %d", &sender,&sender_type) == 2){
                    int fd_parend = ipc_open_for_writing(sender,(DeviceType)sender_type);
                    if(fd_parend != -1){
                        char msg[MAX_MSG_LEN];
                        snprintf(msg, sizeof(msg), "MSG %d", bulb->id);
                        ipc_send_message(fd_parend,msg);
                        close(fd_parend);
                    }
                } else {
                    char msg[MAX_MSG_LEN];
                    snprintf(msg,sizeof(msg),"Device Bulb %d deleted.", bulb->id );
                    ipc_send_controller(STATUS_OK, msg);
                    
                }
                close(fd);
                exit(0);
            }
            //switch
            else if(strncmp(buffer, CMD_SWITCH, strlen(CMD_SWITCH)) == 0) {
                char label[32], pos[32];
                int sender_id = -1; 
                int sender_type = -1;
                int parsed = sscanf(buffer, "%*s %s %s %d %d", label, pos, &sender_id, &sender_type);

                if(strcmp(label, "power") == 0) {
                    bool valid_pos = true;
                    if (strcmp(pos, "on") == 0) {
                        bulb->power = true;
                    } else if (strcmp(pos, "off") == 0) {
                        bulb->power = false;
                    } else {
                        valid_pos = false;
                    }

                    if(parsed >= 4){
                        //il comando viene da un genitore quindi non sporco la shell
                        int fd_parent = ipc_open_for_writing(sender_id, (DeviceType)sender_type);
                        if (fd_parent != -1) {
                            char msg[MAX_MSG_LEN];
                            snprintf(msg, sizeof(msg), "MSG %d", bulb->id);
                            ipc_send_message(fd_parent, msg);
                            close(fd_parent);
                        }
                    } else {
                        //comando dal controller
                        if (valid_pos) {
                            char message[MAX_MSG_LEN];
                            snprintf(message, sizeof(message), "Bulb %d, Power set to: %s", bulb->id, bulb->power ? "on" : "off");
                            ipc_send_controller(STATUS_OK, message);
                        } else {
                            ipc_send_controller(ERR_INVALID_PARAM, "Invalid position for power.");
                        }
                    }
                } else {
                    //se label non valido 
                    if (parsed >= 4) {
                        // Se ci ha chiamato un genitore dobbiamo comunque sbloccarlo
                        int fd_parent = ipc_open_for_writing(sender_id, (DeviceType)sender_type);
                        if (fd_parent != -1) {
                            char msg[MAX_MSG_LEN];
                            snprintf(msg, sizeof(msg), "MSG %d", bulb->id);
                            ipc_send_message(fd_parent, msg);
                            close(fd_parent);
                        }
                    } else {
                        ipc_send_controller(ERR_INVALID_PARAM, "Invalid label for Bulb.");
                    }
                }
            }
            //Info
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
            } 
            //set_parent
            else if(strncmp(buffer , CMD_SET_PARENT, strlen(CMD_SET_PARENT)) == 0){
                continue;
                //TODO: da implementare
            }
            //mirror
            else if(strncmp(buffer , CMD_MIRROR, strlen(CMD_MIRROR)) == 0){

                int sender_id = -1;
                int sender_type = -1;

                //estraggo id dell'hub
                if(sscanf(buffer, "%*s %d %d", &sender_id, &sender_type) == 2){

                    //apro fifo hub in scrittura
                    int fd_sender = ipc_open_for_writing(sender_id, (DeviceType)sender_type);

                    if(fd_sender != -1 ){  
                        char resp[MAX_MSG_LEN];
                        snprintf(resp,sizeof(resp), "%s %d %s ", CMD_MIRROR_RESP, bulb->id, bulb->power ? "On" : "Off");
                        ipc_send_message(fd_sender,resp);
                        close(fd_sender);
                    }
                }

            }
            else {
                ipc_send_controller(ERR_INVALID_COMMAND, " Bulb unknown command.");
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
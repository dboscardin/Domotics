#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "fridge.h"
#include "ipc.h"
#include "device.h"
#include "protocol.h"

#define BUFFER_SIZE 256

Fridge create_fridge_struct(int id) {
    Fridge fridge = {
        .id = id,
        .parent_id = CONTROLLER_ID;
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
        
            //delete
            if (strncmp(buffer, CMD_DELETE, strlen(CMD_DELETE)) == 0){
                int sender = -1;
                int sender_type = -1;
                
                if(sscanf(buffer, "%*s %d %d", &sender,&sender_type) == 2){
                    int fd_parent = ipc_open_for_writing(sender,(DeviceType)sender_type);
                    if(fd_parent != -1){
                        char msg[32];
                        snprintf(msg, sizeof(msg), "MSG %d", fridge->id);
                        ipc_send_message(fd_parent,msg);
                        close(fd_parent);
                    }
                } else {
                    char msg[MAX_MSG_LEN];
                    snprintf(msg,sizeof(msg),"Device Fridge %d deleted.", fridge->id );
                    ipc_send_controller(STATUS_OK, msg);
                    
                }
                close(fd);
                exit(0);
            }
            //Switch
            else if(strncmp(buffer, CMD_SWITCH, strlen(CMD_SWITCH)) == 0) {
                bool is_valid = true;
                bool handled = false;
                char label[32], pos[32];
                int sender_id = -1;
                int sender_type = -1;
                int parsed = sscanf(buffer, "%*s %s %s %d %d", label,pos,&sender_id,&sender_type);

                if(strcmp(label, "is_open") == 0) {
                    if (strcmp(pos, "on") == 0) {
                        fridge->is_open = true;
                    } else if (strcmp(pos, "off") == 0) {
                        fridge->is_open = false;
                    } else {
                        is_valid = false;
                    }
                    handled = true;
                }
                else if(strcmp(label, "time") == 0) {
                    int int_pos = atoi(pos);
                    if (int_pos >= 0) {
                        fridge->time = int_pos;
                    } else {
                        is_valid = false;
                    }
                    handled = true;
                }
                else if(strcmp(label, "delay") == 0) {
                    int int_pos = atoi(pos);
                    if (int_pos >= 0) {
                        fridge->delay = int_pos;
                    } else {
                        is_valid = false;
                    }
                    handled = true;
                }
                //in seguito far sì che perc e therm siano modificati solo manualmente
                else if(strcmp(label, "perc") == 0) {
                    int int_pos = atoi(pos);
                    if (int_pos >= 0 && int_pos <= 100) {
                        fridge->perc = int_pos;
                    } else {
                        is_valid = false;
                    }
                    handled = true;
                }
                else if(strcmp(label, "temp") == 0) {
                    int int_pos = atoi(pos);
                    if (int_pos >= -10 && int_pos <= 50) {
                        fridge->temp = int_pos;
                    } else {
                        is_valid = false;
                    }
                    handled = true;
                }
                else if(strcmp(label, "thermostat") == 0) {
                    int int_pos = atoi(pos);
                    if (int_pos >= -10 && int_pos <= 20) {
                        fridge->thermostat = int_pos;
                    } else {
                        is_valid = false;
                    }
                    handled = true;
                }
                if(handled){
                    if(parsed >= 4){
                        //il comando viene da un genitore
                        int fd_parent = ipc_open_for_writing(sender_id,(DeviceType)sender_type);
                        if(fd_parent != -1){
                            char msg[MAX_MSG_LEN];
                                snprintf(msg, sizeof(msg), "MSG %d", fridge->id);
                                ipc_send_message(fd_parent, msg);
                                close(fd_parent); 
                        }
                    } else {
                        //comando dal controller
                        char message[MAX_MSG_LEN];
                        if (is_valid) {
                           snprintf(message, sizeof(message), "Fridge %d, %s set to: %s\n", fridge->id, label, pos);                            ipc_send_controller(STATUS_OK, message);
                        } else {
                            snprintf(message, sizeof(message), "Invalid position for %s.", label);
                            ipc_send_controller(ERR_INVALID_PARAM, message);
                        }
                    }
                } else {
                    //se label non valido 
                    if (parsed >= 4) {
                        // Se ci ha chiamato un genitore dobbiamo comunque sbloccarlo
                        int fd_parent = ipc_open_for_writing(sender_id, (DeviceType)sender_type);
                        if (fd_parent != -1) {
                            char msg[MAX_MSG_LEN];
                            snprintf(msg, sizeof(msg), "MSG %d", fridge->id);
                            ipc_send_message(fd_parent, msg);
                            close(fd_parent);
                        }
                    } else {
                        ipc_send_controller(ERR_INVALID_PARAM, "Invalid label for Fridge.");
                    }
                }
            }
            //info
            else if(strncmp(buffer , CMD_INFO, strlen(CMD_INFO)) == 0){
                char message[MAX_MSG_LEN];
                char parent[32];

                format_parent_string(fridge->parent_id, parent, sizeof(parent));
                snprintf(message,sizeof(message),
                "\n------- Fridge Details -----\n"
                "ID: %d\n"
                "Door State: %s\n"
                "Time left open: %d s\n"
                "Delay: %d s\n"
                "Fill percentage: %d %%\n"
                "Current Temp: %d °C\n"
                "Thermostat: %d °C\n"
                "----------------------------\n",
                fridge->id,fridge->is_open ? "Open" : "Closed",fridge->time,fridge->delay,fridge->perc,fridge->temp,fridge->thermostat);
                ipc_send_controller(STATUS_OK, message);
            }
            //set_parent
            else if(strncmp(buffer , CMD_SET_PARENT, strlen(CMD_SET_PARENT)) == 0){
                handle_set_parent(&fridge->parent_id, buffer);
            }
            //mirror
            else if(strncmp(buffer , CMD_MIRROR, strlen(CMD_MIRROR)) == 0){

                int sender_id;
                int sender_type;

                //estraggo id dell'hub
                if(sscanf(buffer, "%*s %d %d", &sender_id, &sender_type) == 2){

                    //apro fifo hub in scrittura
                    int fd_sender = ipc_open_for_writing(sender_id, sender_type);

                    if(fd_sender != -1 ){
                        char resp[MAX_MSG_LEN];
                        snprintf(resp,sizeof(resp), "%s %d %s ", CMD_MIRROR_RESP, fridge->id, fridge->is_open ? "Open" : "Closed");
                        ipc_send_message(fd_sender,resp);
                        close(fd_sender);
                    }
                }

            }
            else {
                ipc_send_controller(ERR_INVALID_COMMAND, "Fridge unknown command.");
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
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

Bulb create_bulb_struct(int id) {
    Bulb bulb = {
        .id = id,
        .parent_id = CONTROLLER_ID,
        .power = false,
        .time = 0,
        .tracking = false
    };

    return bulb;
}

void bulb_run(Bulb *bulb) {

    // genero un valore casuale unico basato sul PID
    srand(time(NULL) ^ getpid());

    // apro la fifo in lettura
    int fd = ipc_open_for_listening(bulb->id, DEVICE_BULB);
    char buffer[MAX_MSG_LEN];

    while(1) {

        // leggo messaggio dalla fifo
        int bytes = ipc_read_line(fd, buffer, sizeof(buffer));
        if (bytes > 0) {


            // DELETE
            if (strncmp(buffer, CMD_DELETE, strlen(CMD_DELETE)) == 0){
                int sender = -1;
                int sender_type = -1;
                
                if(sscanf(buffer, "%*s %d %d", &sender,&sender_type) == 2){
                    // se arriva da un genitore
                    int fd_parent = ipc_open_for_writing(sender,(DeviceType)sender_type);
                    if(fd_parent != -1){
                        char message[MAX_MSG_LEN];
                        snprintf(message, sizeof(message), "MSG %d", bulb->id);
                        ipc_send_message(fd_parent, message);
                        close(fd_parent);
                    }
                } else {
                    //se arriva dal controller
                    char message[MAX_MSG_LEN];
                    snprintf(message, sizeof(message),"Device Bulb %d deleted.", bulb->id );
                    ipc_send_controller(STATUS_OK, message);
                    
                }
                close(fd);
                exit(0);
            }
            // SWITCH
            else if(strncmp(buffer, CMD_SWITCH, strlen(CMD_SWITCH)) == 0) {

                // delay
                ipc_simulate_delay();
                
                char label[32], pos[32];
                int sender_id = -1; 
                int sender_type = -1;
                int parsed = sscanf(buffer, "%*s %s %s %d %d", label, pos, &sender_id, &sender_type);

                if(strcmp(label, "power") == 0) {
                    bool valid_pos = true;
                    if (strcmp(pos, "on") == 0) {
                        bulb->power = true;
                        // inizia a contare il tempo
                        clock_gettime(CLOCK_MONOTONIC, &bulb->active_since);
                        bulb->tracking = true;
                    } else if (strcmp(pos, "off") == 0) {
                        if (bulb->tracking) {
                            // Quando si spegne, calcola i secondi trascorsi e li somma al totale
                            long elapsed = compute_elapsed_seconds(&bulb->active_since);
                            bulb->time += elapsed;
                            bulb->tracking = false;
                        }
                        bulb->power = false;
                        
                    } else {
                        valid_pos = false;
                    }

                    if(parsed >= 4){
                        //il comando viene da un genitore
                        int fd_parent = ipc_open_for_writing(sender_id, (DeviceType)sender_type);
                        if (fd_parent != -1) {
                            char message[MAX_MSG_LEN];
                            snprintf(message, sizeof(message), "MSG %d", bulb->id);
                            ipc_send_message(fd_parent, message);
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
                            char message[MAX_MSG_LEN];
                            snprintf(message, sizeof(message), "MSG %d", bulb->id);
                            ipc_send_message(fd_parent, message);
                            close(fd_parent);
                        }
                    } else {
                        ipc_send_controller(ERR_INVALID_PARAM, "Invalid label for Bulb.");
                    }
                }
            }
            // INFO
            else if(strncmp(buffer , CMD_INFO, strlen(CMD_INFO)) == 0){
                char message[MAX_MSG_LEN];
                char parent[32];

                // calcolo dinamico del tempo
                long total_time = bulb->time;
                if (bulb->tracking) {
                    total_time += compute_elapsed_seconds(&bulb->active_since);
                }

                format_parent_string(bulb->parent_id, parent, sizeof(parent));

                snprintf(message,sizeof(message),
                "\n-------- Bulb Details ------\n"
                "ID: %d\n"
                "State: %s\n"
                "Total usage time: %ld seconds\n"
                "Linked to: %s\n"
                "----------------------------\n", 
                bulb->id, bulb->power ? "On" : "Off", total_time, parent);
                
                ipc_send_controller(STATUS_OK, message);
            } 
            // SET_PARENT
            else if(strncmp(buffer , CMD_SET_PARENT, strlen(CMD_SET_PARENT)) == 0){
                handle_set_parent(&bulb->parent_id, buffer);
            }
            // MIRROR
            else if(strncmp(buffer , CMD_MIRROR, strlen(CMD_MIRROR)) == 0){

                int sender_id = -1;
                int sender_type = -1;

                //estraggo id del parent
                if(sscanf(buffer, "%*s %d %d", &sender_id, &sender_type) == 2){

                    //apro fifo parent in scrittura
                    int fd_sender = ipc_open_for_writing(sender_id, (DeviceType)sender_type);

                    if(fd_sender != -1 ){  
                        // rispodno inviando lo stato attuale
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
            // se la fifo è vuota sospendo il processo per 50ms per evitare di consumare il 100% di CPU
            usleep(50000);
        }
    }
    close(fd);
}

void create_bulb(int id) {
    Bulb b = create_bulb_struct(id);
    bulb_run(&b);
}
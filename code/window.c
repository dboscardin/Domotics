#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "window.h"
#include "ipc.h"
#include "device.h"
#include "protocol.h"

Window create_window_struct(int id) {
    Window window = {
        .id = id,
        .parent_id = CONTROLLER_ID,
        .is_open = false,
        .time = 0,
        .tracking = false
    };
    return window;
} 

void window_run(Window *window) {

    srand(time(NULL) ^ getpid());

    int fd = ipc_open_for_listening(window->id, DEVICE_WINDOW);
    char buffer[MAX_MSG_LEN];
    while(1) {

        int bytes = ipc_read_line(fd, buffer, sizeof(buffer));
        if (bytes > 0) {

            //delete
            if (strncmp(buffer, CMD_DELETE, strlen(CMD_DELETE)) == 0){
                int sender = -1;
                int sender_type = -1;
                
                if(sscanf(buffer, "%*s %d %d", &sender,&sender_type) == 2){
                    int fd_parent = ipc_open_for_writing(sender,(DeviceType)sender_type);
                    if(fd_parent != -1){
                        char message[32];
                        snprintf(message, sizeof(message), "MSG %d", window->id);
                        ipc_send_message(fd_parent, message);
                        close(fd_parent);
                    }
                } else {
                    char message[MAX_MSG_LEN];
                    snprintf(message ,sizeof(message),"Device Window %d deleted.", window->id );
                    ipc_send_controller(STATUS_OK, message);
                    
                }
                close(fd);
                exit(0);
            }
            //switch
            else if(strncmp(buffer, CMD_SWITCH, strlen(CMD_SWITCH)) == 0) {
                //delay
                ipc_simulate_delay();
                bool handled = false;
                bool valid_pos = true;
                char label[32], pos[32];
                int sender_id = -1;
                int sender_type = -1;
                int parsed = sscanf(buffer, "%*s %s %s %d %d", label, pos, &sender_id, &sender_type);

                if(strcmp(label, "open") == 0) {
                    if (strcmp(pos, "on") == 0) {
                        if (!window->tracking) {
                            clock_gettime(CLOCK_MONOTONIC, &window->active_since);
                            window->tracking = true;
                        }
                        window->is_open = true;
                    } else if(strcmp(pos, "off") == 0) {
                        // Switch a molla: ritorna automaticamente a off
                    } else {
                        valid_pos = false;
                    }
                    handled = true;
                }
                else if(strcmp(label, "close") == 0) {
                    if (strcmp(pos, "on") == 0) {
                        if (window->tracking) {
                            long elapsed = compute_elapsed_seconds(&window->active_since);
                            window->time += elapsed;
                            window->tracking = false;
                        }
                        window->is_open = false;
                    } else if (strcmp(pos, "off") == 0) {
                        // Switch a molla: ritorna automaticamente a off
                    } else {
                        valid_pos = false;
                    }
                    handled = true;
                }

                if (handled) {
                    if(parsed >= 4){
                        // Il comando viene da un genitore
                        int fd_parent = ipc_open_for_writing(sender_id, (DeviceType)sender_type);
                        if(fd_parent != -1){
                            char message[MAX_MSG_LEN];
                            snprintf(message, sizeof(message), "MSG %d", window->id);
                            ipc_send_message(fd_parent, message);
                            close(fd_parent); 
                        }
                    } else {
                        // Comando dal controller
                        char message[MAX_MSG_LEN];
                        if(valid_pos) {
                            snprintf(message, sizeof(message), "Window %d, %s set to: %s", window->id, label, pos);
                            ipc_send_controller(STATUS_OK, message);
                        } else {
                            snprintf(message, sizeof(message),"Invalid position for %s.", label);
                            ipc_send_controller(ERR_INVALID_PARAM, message);
                        }
                    }
                } else {
                    // Se la label non è valida
                    if (parsed >= 4) {
                        int fd_parent = ipc_open_for_writing(sender_id, (DeviceType)sender_type);
                        if (fd_parent != -1) {
                            char message[MAX_MSG_LEN];
                            snprintf(message, sizeof(message), "MSG %d", window->id);
                            ipc_send_message(fd_parent, message);
                            close(fd_parent);
                        }
                    } else {
                        ipc_send_controller(ERR_INVALID_PARAM, "Invalid label for Window.");
                    }
                }
            }
            //info
            else if(strncmp(buffer , CMD_INFO, strlen(CMD_INFO)) == 0){
                char message[MAX_MSG_LEN];
                char parent[32];

                long total_time = window->time;
                if (window->tracking) {
                    total_time += compute_elapsed_seconds(&window->active_since);
                }

                format_parent_string(window->parent_id, parent, sizeof(parent));

                snprintf(message,sizeof(message),
                "\n------- Window Details -----\n"
                "ID: %d\n"
                "State: %s\n" 
                "Time left open: %ld s\n"
                "Linked to: %s\n"
                "----------------------------\n",
                window->id, window->is_open ? "Open" : "Closed", total_time, parent );
                ipc_send_controller(STATUS_OK, message);
            }
            //set_parent
            else if(strncmp(buffer , CMD_SET_PARENT, strlen(CMD_SET_PARENT)) == 0){
                handle_set_parent(&window->parent_id, buffer);
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
                        snprintf(resp, sizeof(resp), "%s %d %s ", CMD_MIRROR_RESP, window->id, window->is_open ? "Open" : "Closed");
                        ipc_send_message(fd_sender,resp);
                        close(fd_sender);
                    }
                }

            } 
            else {
                ipc_send_controller(ERR_INVALID_COMMAND, " Window unknown command.");
            }
        } else {
        usleep(50000); // il processo consuma meno risorse
        }
    }
    close(fd);
}

void create_window(int id) {
    Window w = create_window_struct(id);
    window_run(&w);
    
}
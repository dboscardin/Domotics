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

static void get_timer_state(TimerDevice *timer, int fd_ascolto, char *out_state, size_t state_len) {
    if (timer->num_children == 0) {
        snprintf(out_state, state_len, "None");
        return;
    }

    snprintf(out_state, state_len, "Unknown");
    int child_id = timer->children[0].id;
    DeviceType child_type = timer->children[0].type;

    int fd_child = ipc_open_for_writing(child_id, child_type);
    if (fd_child != -1) {
        char cmd[32];
        snprintf(cmd, sizeof(cmd), "%s %d %d", CMD_MIRROR, timer->id, DEVICE_TIMER);
        ipc_send_message(fd_child, cmd);
        close(fd_child);
    }

    int timeout = 1000; 
    while (timeout > 0) {
        char buf[4096];
        if (ipc_read_line(fd_ascolto, buf, sizeof(buf)) > 0) {
            char *ptr = buf;
            while ((ptr = strstr(ptr, CMD_MIRROR_RESP)) != NULL) {
                int resp_id;
                char resp_state[64];
                
                if (sscanf(ptr, "%*s %d %s", &resp_id, resp_state) == 2) {      
                    if (resp_id == child_id) {
                        snprintf(out_state, state_len, "%s", resp_state);
                        return; 
                    }
                }
                ptr += strlen(CMD_MIRROR_RESP);
            }
        } else {
            usleep(50000);
            timeout--;
        }
    }
}


void timer_run(TimerDevice *timer){

    srand(time(NULL) ^ getpid());

    int fd_ascolto = ipc_open_for_listening(timer->id,DEVICE_TIMER);
    char buffer[BUFFER_SIZE];

    char last_triggered[6] = ""; //serve per non mandare il comando di switch un sacco di volte e far ricordare al timer che l'ha già mandato

    while(1){


        //logica timer

        time_t rawtime;
        struct tm *timeinfo;
        char current_time[6];
        time(&rawtime);
        timeinfo = localtime(&rawtime);
        strftime(current_time,sizeof(current_time), "%H:%M", timeinfo);
        

        if(timer->num_children > 0 && strlen(timer->begin) > 0 && strlen(timer->end) > 0){
            int child_id = timer->children[0].id;
            DeviceType child_type = timer->children[0].type;

            //controllo il device
            const char *labelOn = "power";
            const char *labelOff = "power";
            const char *actionOff = "off"; // Azione standard di spegnimento
            
            if (child_type == DEVICE_WINDOW || child_type == DEVICE_FRIDGE) {
                labelOn = "open";
                labelOff = "close";
                actionOff = "on"; // Finestre e frighi si chiudono premendo il tasto close
            }

            if (strcmp(current_time, timer->begin) == 0 && strcmp(last_triggered, timer->begin) != 0) {
                int fd_child = ipc_open_for_writing(child_id, child_type);
                if (fd_child != -1) {
                    char cmd[64];
                    snprintf(cmd, sizeof(cmd), "%s %s on %d %d", CMD_SWITCH, labelOn, timer->id, DEVICE_TIMER);
                    ipc_send_message(fd_child, cmd); 
                    close(fd_child);

                    //aspetto un risposta dal figlio
                    int msg = 0;
                    int timeout = 1000;
                    while (msg < 1 && timeout > 0) {
                        char msg_buf[64];
                        int n = ipc_read_line(fd_ascolto, msg_buf, sizeof(msg_buf));
                        if (n > 0 && strncmp(msg_buf, "MSG", 3) == 0) msg++;
                        else { usleep(10000); timeout--; }
                    }

                    //notifico il controller
                    char alert[MAX_MSG_LEN];
                    snprintf(alert, sizeof(alert), "\nTimer %d automatically switched its device on at %s", timer->id, current_time);
                    ipc_send_controller(STATUS_OK, alert);

                }
                snprintf(last_triggered, sizeof(last_triggered), "%s", current_time);

            } 
            else if (strcmp(current_time, timer->end) == 0 && strcmp(last_triggered, timer->end) != 0) {
                int fd_child = ipc_open_for_writing(child_id, child_type);
                if (fd_child != -1) {
                    char cmd[64];
                    snprintf(cmd, sizeof(cmd), "%s %s %s %d %d", CMD_SWITCH, labelOff, actionOff, timer->id, DEVICE_TIMER);
                    ipc_send_message(fd_child, cmd);
                    close(fd_child);

                    //aspetto un risposta dal figlio
                    int msg = 0;
                    int timeout = 1000;
                    while (msg < 1 && timeout > 0) {
                        char msg_buf[64];
                        int n = ipc_read_line(fd_ascolto, msg_buf, sizeof(msg_buf));
                        if (n > 0 && strncmp(msg_buf, "MSG", 3) == 0) msg++;
                        else { usleep(10000); timeout--; }
                    }

                    //notifico il controller
                    char alert[MAX_MSG_LEN];
                    snprintf(alert, sizeof(alert), "\nTimer %d automatically switched its device OFF at %s", timer->id, current_time);
                    ipc_send_controller(STATUS_OK, alert);
                }
                snprintf(last_triggered, sizeof(last_triggered), "%s", current_time);
            }
        }


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
            //setparent
            else if(strncmp(buffer, CMD_SET_PARENT, strlen(CMD_SET_PARENT)) == 0){
                int p_id;
                if (sscanf(buffer, "%*s %d", &p_id) == 1){ 
                    timer->parent_id = p_id;
                }
                continue;
            }
            //unlink
            else if(strncmp(buffer, CMD_UNLINK_CHILD, strlen(CMD_UNLINK_CHILD)) == 0){
                
                int child_id;
                if (sscanf(buffer, "%*s %d", &child_id) == 1){
                    if (timer->num_children > 0 && timer->children[0].id == child_id) {

                        //avviso i figli di toglermi come padre
                        int fd_child = ipc_open_for_writing(child_id,timer->children[0].type);
                        if(fd_child != -1){
                            char cmd_unlink[64];
                            snprintf(cmd_unlink, sizeof(cmd_unlink), "%s -1", CMD_SET_PARENT);
                            ipc_send_message(fd_child, cmd_unlink);
                            close(fd_child);
                        }

                        //rimuovo il figlio
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
                char label[32], pos[32];
                int sender_id = -1, sender_type = -1;
                int parsed = sscanf(buffer, "%*s %s %s %d %d", label, pos, &sender_id, &sender_type);

                if (strcmp(label, "begin") == 0 || strcmp(label, "end") == 0) {
                    int h, m;
                    if (sscanf(pos, "%d:%d", &h, &m) == 2 && h >= 0 && h <= 23 && m >= 0 && m <= 59) {

                        //prendo lora attuale per vedere se è passato
                        time_t rawtime; struct tm *timeinfo; char current_time[6];
                        time(&rawtime); timeinfo = localtime(&rawtime);
                        strftime(current_time, sizeof(current_time), "%H:%M", timeinfo);

                        bool valid = true;
                        char err_msg[64] = "";


                        if (strcmp(pos, current_time) < 0) {
                            valid = false;
                            snprintf(err_msg, sizeof(err_msg), "Error: Time %s is in the past!", pos);
                        } else if (strcmp(label, "begin") == 0 && strlen(timer->end) > 0 && strcmp(pos, timer->end) >= 0) {
                            valid = false;
                            snprintf(err_msg, sizeof(err_msg), "Error: Begin time cannot be >= End time.");
                        } else if (strcmp(label, "end") == 0 && strlen(timer->begin) > 0 && strcmp(timer->begin, pos) >= 0) {
                            valid = false;
                            snprintf(err_msg, sizeof(err_msg), "Error: End time cannot be <= Begin time.");
                        }

                        if(valid){
                            if (strcmp(label, "begin") == 0) {
                                snprintf(timer->begin, sizeof(timer->begin), "%02d:%02d", h, m);
                                if (parsed < 3) ipc_send_controller(STATUS_OK, "Timer begin set.");
                            } else {
                                snprintf(timer->end, sizeof(timer->end), "%02d:%02d", h, m);
                                if (parsed < 3) ipc_send_controller(STATUS_OK, "Timer end set.");
                            }
                        } else {
                            if (parsed < 3) ipc_send_controller(ERR_INVALID_PARAM, err_msg);
                        }
                    } else {
                        if (parsed < 3) ipc_send_controller(ERR_INVALID_PARAM, "Invalid time format! Use HH:MM.");
                    }
                    
                    if (parsed >= 3) {
                        int fd_parent = ipc_open_for_writing(sender_id, (DeviceType)sender_type);
                        if(fd_parent != -1) {
                            char cmd[32];
                            snprintf(cmd, sizeof(cmd), "MSG %d", timer->id);
                            ipc_send_message(fd_parent, cmd);
                            close(fd_parent);
                        }
                    }
                } 
                else {
                    if (timer->num_children > 0) {
                        int child_id = timer->children[0].id;
                        DeviceType child_type = timer->children[0].type;

                        //controllo il device
                        const char *out_label = label;
                        const char *out_pos = pos;
                        if (child_type == DEVICE_WINDOW || child_type == DEVICE_FRIDGE) {
                            if (strcmp(pos, "on") == 0) {
                                out_label = "open";
                                out_pos = "on";
                            } else {
                                out_label = "close";
                                out_pos = "on";
                            }
                        }
                        
                        int fd_child = ipc_open_for_writing(child_id, child_type);
                        if (fd_child != -1) {
                            char cmd[64];
                            snprintf(cmd, sizeof(cmd), "%s %s %s %d %d", CMD_SWITCH, out_label, out_pos, timer->id, DEVICE_TIMER);
                        }
                        
                        int msg = 0;
                        int timeout = 1000;
                        while (msg < 1 && timeout > 0) {
                            char msg_buf[64];
                            int n = ipc_read_line(fd_ascolto, msg_buf, sizeof(msg_buf));
                            if (n > 0 && strncmp(msg_buf, "MSG", 3) == 0){
                                msg++;
                            } else { 
                                usleep(10000); timeout--; 
                            }
                        }
                    }

                    if (parsed >= 3) {
                        int fd_parent = ipc_open_for_writing(sender_id, (DeviceType)sender_type);
                        if(fd_parent != -1) {
                            char ack[32];
                            snprintf(ack, sizeof(ack), "MSG %d", timer->id);
                            ipc_send_message(fd_parent, ack);
                            close(fd_parent);
                        }
                    } else {
                        ipc_send_controller(STATUS_OK, "Timer forwarded switch command.");
                    }
                }

            }
            //delete
            else if(strncmp(buffer, CMD_DELETE, strlen(CMD_DELETE)) == 0){

                int sender_id = -1;
                int sender_type = -1;

                if (timer->num_children > 0) {
                    int child_id = timer->children[0].id;
                    DeviceType child_type = timer->children[0].type;
                    
                    int fd_child = ipc_open_for_writing(child_id, child_type);
                    if (fd_child != -1) {
                        char cmd[64];
                        snprintf(cmd, sizeof(cmd), "%s %d %d", CMD_DELETE, timer->id, DEVICE_TIMER);
                        ipc_send_message(fd_child, cmd);
                        close(fd_child);
                    }

                    //aspetta che i figli vengano eliminati
                    int deleted = 0;
                    int timeout = 1000;
                    while (deleted < 1 && timeout > 0) {
                        char deleted_buf[64];
                        int n = ipc_read_line(fd_ascolto, deleted_buf, sizeof(deleted_buf));
                        if (n > 0 && strncmp(deleted_buf, "MSG", 3) == 0) {
                            deleted++;
                        } else {
                            usleep(10000);
                            timeout--;
                        }
                    }
                }

                //Rispondo a chi mi ha eliminato
                if (sscanf(buffer, "%*s %d %d", &sender_id, &sender_type) == 2) {
                    int fd_parent = ipc_open_for_writing(sender_id, (DeviceType)sender_type);
                    if (fd_parent != -1) {
                        char msg[32];
                        snprintf(msg, sizeof(msg), "MSG %d", timer->id);
                        ipc_send_message(fd_parent, msg);
                        close(fd_parent);
                    }
                } else {
                    char msg[MAX_MSG_LEN];
                    int offset = 0;
                    offset += snprintf(msg + offset, sizeof(msg) - offset, "Timer %d and all its children deleted.\n", timer->id);
                    if (timer->num_children > 0) {
                        offset += snprintf(msg + offset, sizeof(msg) - offset, "Device %s %d deleted.\n", get_device_type_name(timer->children[0].type), timer->children[0].id);
                    }
                    ipc_send_controller(STATUS_OK, msg);
                }
                
                close(fd_ascolto);
                exit(0);

            }
            //info
            else if(strncmp(buffer, CMD_INFO, strlen(CMD_INFO)) == 0){
                char message[MAX_MSG_LEN];
                int offset = 0;

                offset += snprintf(message + offset, sizeof(message) - offset,
                "\n------- Timer Details ------\nID: %d\n", timer->id);

                if (timer->parent_id == -1) offset += snprintf(message + offset, sizeof(message) - offset, "Linked to Hub: no\n");
                else offset += snprintf(message + offset, sizeof(message) - offset, "Linked to Hub ID: %d\n", timer->parent_id);

                offset += snprintf(message + offset, sizeof(message) - offset, "Schedule: %s -> %s\n", 
                strlen(timer->begin) > 0 ? timer->begin : "Not set",
                strlen(timer->end) > 0 ? timer->end : "Not set");

                if (timer->num_children == 0) {
                    offset += snprintf(message + offset, sizeof(message) - offset, "Linked Device: none\n");
                } else {
                    char child_state[64];
                    get_timer_state(timer, fd_ascolto, child_state, sizeof(child_state));

                    offset += snprintf(message + offset, sizeof(message) - offset, "Linked Device ID: %d | Type: %s | State: %s\n", 
                        timer->children[0].id, get_device_type_name(timer->children[0].type), child_state);
                    
                    if (strstr(child_state, "Manual_Override")) {
                        offset += snprintf(message + offset, sizeof(message) - offset, "Manual_Override\n");
                    }
                }
                
                offset += snprintf(message + offset, sizeof(message) - offset, "----------------------------\n");
                ipc_send_controller(STATUS_OK, message);

            }
            //Mirror
            else if (strncmp(buffer, CMD_MIRROR, strlen(CMD_MIRROR)) == 0) {
                int p_sender_id;
                int p_sender_type;
                if (sscanf(buffer, "%*s %d %d", &p_sender_id, &p_sender_type) == 2) {
                    char child_state[64];
                    get_timer_state(timer, fd_ascolto, child_state, sizeof(child_state));
                    
                    int fd_parent = ipc_open_for_writing(p_sender_id, p_sender_type);
                    if (fd_parent != -1) {
                        char resp[MAX_MSG_LEN];
                        snprintf(resp, sizeof(resp), "%s %d %s ", CMD_MIRROR_RESP, timer->id, child_state);
                        ipc_send_message(fd_parent, resp);
                        close(fd_parent);
                    }
                }
            } else if (strncmp(buffer, CMD_CHILD_DIED, strlen(CMD_CHILD_DIED)) == 0) {
                int dead_id;
                if (sscanf(buffer, "%*s %d", &dead_id) == 1) {
                    if (timer->num_children > 0 && timer->children[0].id == dead_id) {
                        timer->num_children = 0;
                    }
                }
            } else {
                ipc_send_controller(ERR_INVALID_COMMAND,"Timer unknown command.");
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
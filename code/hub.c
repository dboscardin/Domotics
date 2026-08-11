#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include "hub.h"
#include "ipc.h"
#include "device.h"
#include "protocol.h"

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

HubDevice create_hub_struct(int id) {
    HubDevice hub = {
        .id = id,
        .parent_id = -1,
        .num_children = 0
    };
    return hub;
}

int hub_add_child(HubDevice *hub, int child_id, DeviceType child_type) {
    if (hub->num_children >= MAX_CHILDREN) {
        return -1;
    }

    // Controlliamo se il dispositivo è già stato aggiunto
    for (int i = 0; i < hub->num_children; i++) {
        if (hub->children[i].id == child_id) {
            return 1;
        }
    }

    // Colleghiamo il nuovo dispositivo figlio
    hub->children[hub->num_children].id = child_id;
    hub->children[hub->num_children].type = child_type;
    hub->num_children++;

    return 0;
}

int hub_remove_child(HubDevice *hub, int child_id){
    int index = -1;

    for (int i = 0; i < hub->num_children; i++) {
        if (hub->children[i].id == child_id) {
            index = i;
            break;
        }
    }

    if (index == -1) {
        return -1;
    }

    // Shift degli elementi verso sinistra per coprire il buco
    for (int i = index; i < hub->num_children - 1; i++) {
        hub->children[i] = hub->children[i + 1];
    }

    hub->num_children--;
    return 0;
}

static void get_state(HubDevice *hub, int fd_ascolto, char child_states[MAX_CHILDREN][64], char *overall_state, size_t state_len) {
    if (hub->num_children == 0) {
        snprintf(overall_state, state_len, "None");
        return;
    }

    //Invio MIRROR
    for (int i = 0; i < hub->num_children; i++) {
        snprintf(child_states[i], 64, "Unknown");
        
        int fd_child = ipc_open_for_writing(hub->children[i].id, hub->children[i].type);
        if (fd_child != -1) {
            char cmd[32];
            snprintf(cmd, sizeof(cmd), "%s %d %d", CMD_MIRROR, hub->id, DEVICE_HUB);
            ipc_send_message(fd_child, cmd);
            close(fd_child);
        }
    }

    //Aspetto le risposte dai figli
    int received = 0;
    int timeout = 300; 

    while (received < hub->num_children && timeout > 0) {
        char buf[4096];
        if (ipc_read_line(fd_ascolto, buf, sizeof(buf)) > 0) {
            char *ptr = buf;
            while ((ptr = strstr(ptr, CMD_MIRROR_RESP)) != NULL) {
                int id;
                char state[64];    
                
                if (sscanf(ptr, "%*s %d %s", &id, state) == 2) {
                    for (int i = 0; i < hub->num_children; i++) {
                        if (hub->children[i].id == id && strcmp(child_states[i], "Unknown") == 0) {
                            snprintf(child_states[i], 64, "%s", state);
                            received++;
                            break;
                        }
                    }
                }
                ptr += strlen(CMD_MIRROR_RESP);
            }
        } else {
            usleep(50000);
            timeout--;
        }
    }

    //calcolo dello stato
    bool is_active = false;
    bool is_inactive = false;

    for (int i = 0; i < hub->num_children; i++) {
        if (strstr(child_states[i], "Manual_Override")) {
            is_active = is_inactive = true;
        } else if (strstr(child_states[i], "On") || strstr(child_states[i], "Open") || strstr(child_states[i], "Active")) {
            is_active = true;
        } else if (strstr(child_states[i], "Off") || strstr(child_states[i], "Closed") || strstr(child_states[i], "Inactive")) {
            is_inactive = true;
        }
    }

    if (is_active && is_inactive){
        snprintf(overall_state, state_len, "Manual_Override");
    } else if (is_active){           
        snprintf(overall_state, state_len, "Active");
    } else if (is_inactive){         
        snprintf(overall_state, state_len, "Inactive");
    } else{                         
        snprintf(overall_state, state_len, "Unknown");
    }

}

void hub_run(HubDevice *hub){
    srand(time(NULL) ^ getpid());

    // Apertura FIFO ascolto
    int fd_ascolto = ipc_open_for_listening(hub->id, DEVICE_HUB);

    // Ricezione messaggi
    char buffer[256];
    while (1) {
        // Lettura FIFO
        int bytes_letti = ipc_read_line(fd_ascolto, buffer, sizeof(buffer));

        if (bytes_letti > 0) {

            //delay
            ipc_simulate_delay();

            //link
            if (strncmp(buffer, CMD_LINK_CHILD, strlen(CMD_LINK_CHILD)) == 0) {
                int child_id;
                int child_type;

                if (sscanf(buffer, "%*s %d %d", &child_id, &child_type) == 2) {

                    int res = hub_add_child(hub, child_id, (DeviceType)child_type);
                    if(res == 0){
                        char msg[MAX_MSG_LEN];
                        snprintf(msg,sizeof(msg), "Link completed: Device %d is now child of %d.", child_id, hub->id);
                        ipc_send_controller(STATUS_OK,msg);
                    } 
                    else if(res == 1){
                        char msg[MAX_MSG_LEN];
                        snprintf(msg, sizeof(msg), "Notice: Device %d is Already linked to Hub %d.", child_id, hub->id);
                        ipc_send_controller(ERR_INVALID_PARAM, msg);
                    }
                    else {
                        ipc_send_controller(ERR_LINK_FAILED, "Failed to link: max capacity reached.");
                    }
                } else {
                    ipc_send_controller(ERR_INVALID_PARAM, "Invalid link format.");
                }
            
            } 
            //setparent
            else if(strncmp(buffer, CMD_SET_PARENT, strlen(CMD_SET_PARENT)) == 0){
                int p_id;
                if (sscanf(buffer, "%*s %d", &p_id) == 1) {
                    hub->parent_id = p_id;
                }
                continue;
            }
            //unlink
            else if(strncmp(buffer, CMD_UNLINK_CHILD, strlen(CMD_UNLINK_CHILD)) == 0){
                int child_id;

                if (sscanf(buffer, "%*s %d", &child_id) == 1){
                    int child_idx = -1;
                    for (int i = 0; i < hub->num_children; i++) {
                        if (hub->children[i].id == child_id) {
                            child_idx = i;
                            break;
                        }
                    }

                    if (child_idx != -1) {
                        int fd_child = ipc_open_for_writing(child_id, hub->children[child_idx].type);
                        if (fd_child != -1) {
                            char cmd_unlink[64];
                            snprintf(cmd_unlink, sizeof(cmd_unlink), "%s -1", CMD_SET_PARENT);
                            ipc_send_message(fd_child, cmd_unlink);
                            close(fd_child);
                        }

                        int res = hub_remove_child(hub, child_id);
                        if (res == 0) {
                            char msg[MAX_MSG_LEN];
                            snprintf(msg, sizeof(msg), "Unlink completed: Device %d removed from Hub %d.", child_id, hub->id);
                            ipc_send_controller(STATUS_OK, msg);
                        } else {
                            char msg[MAX_MSG_LEN];
                            snprintf(msg, sizeof(msg), "Notice: Device %d is not linked to Hub %d.", child_id, hub->id);
                            ipc_send_controller(ERR_NOT_FOUND, msg);
                        }
                    } else {
                        char msg[MAX_MSG_LEN];
                        snprintf(msg, sizeof(msg), "Notice: Device %d is not linked to Hub %d.", child_id, hub->id);
                        ipc_send_controller(ERR_NOT_FOUND, msg);
                    }
                } else {
                    fprintf(stderr, "Error: invalid UNLINK format.\n");
                }
            //delete        
            } else if(strncmp(buffer, CMD_DELETE, strlen(CMD_DELETE)) == 0) {

                int sender_id = -1;
                int sender_type = -1;

                for(int i=0; i < hub->num_children; i++){
                    int child_id = hub->children[i].id;
                    DeviceType child_type = hub->children[i].type;

                    int fd_child = ipc_open_for_writing(child_id,child_type);
                    if(fd_child != -1){
                        char cmd[64];
                        snprintf(cmd, sizeof(cmd), "%s %d %d", CMD_DELETE, hub->id, DEVICE_HUB);
                        ipc_send_message(fd_child, cmd);
                        close(fd_child);
                    }
                }

                int deleted = 0;
                int timeout = 300; //timer per evitare di stare nel loop in caso un figlio sia crashato
                while (deleted < hub->num_children && timeout > 0) {
                    char deleted_buf[64];
                    int n = ipc_read_line(fd_ascolto, deleted_buf, sizeof(deleted_buf));
                    if (n > 0 && strncmp(deleted_buf, "MSG", 3) == 0) {
                        deleted++;
                    } else {
                        usleep(10000); //10ms
                        timeout--;
                    }
                }

                //rispondo a chi mi ha eliminato
                if (sscanf(buffer, "%*s %d %d", &sender_id,&sender_type) == 2) {
                    int fd_parent = ipc_open_for_writing(sender_id, (DeviceType)sender_type);
                    if (fd_parent != -1) {
                        char msg[32];
                        snprintf(msg, sizeof(msg), "MSG %d", hub->id);
                        ipc_send_message(fd_parent, msg);
                        close(fd_parent);
                    }
                } else {
                    char msg[MAX_MSG_LEN];
                    snprintf(msg,sizeof(msg), "Hub %d and all its children deleted.", hub->id);
                    ipc_send_controller(STATUS_OK,msg); 
                }

                close(fd_ascolto);
                exit(0);
            }
            //switch
            else if (strncmp(buffer, "SWITCH", 6) == 0) {

                char label[32], pos[32];
                int sender_id = -1, sender_type = -1;
                int parsed = sscanf(buffer, "%*s %s %s %d %d", label, pos, &sender_id, &sender_type);

                for (int i = 0; i < hub->num_children; i++) {
                    int child_id = hub->children[i].id;
                    DeviceType child_type = hub->children[i].type;

                    //Traduzione del comando
                    const char *out_label = label;
                    if (child_type == DEVICE_WINDOW || child_type == DEVICE_FRIDGE) {
                        out_label = "is_open";
                    }

                    int fd_child = ipc_open_for_writing(child_id, child_type);
                    if (fd_child != -1) {
                        char cmd[64];
                        snprintf(cmd, sizeof(cmd), "%s %s %s %d %d", CMD_SWITCH, out_label, pos, hub->id, DEVICE_HUB);
                        ipc_send_message(fd_child, cmd);
                        close(fd_child);
                    } 
                }

                //aspetto che tutti i figli rispondano
                int msg = 0;
                int timeout = 300;
                while (msg < hub->num_children && timeout > 0) {
                    char msg_buf[64];
                    int n = ipc_read_line(fd_ascolto, msg_buf, sizeof(msg_buf));
                    if (n > 0 && strncmp(msg_buf, "MSG", 3) == 0) {
                        msg++;
                    } else {
                        usleep(10000);
                        timeout--;
                    }
                }
                
                
                // controllo se il messagio di switch è mandato dal controller oppure da un hub/timer cosi da dirgli che ho finito il mio lavoro
                if ( parsed >= 3) {
                    int parent_type = (parsed == 4) ? sender_type : DEVICE_HUB;
                    int fd_parent = ipc_open_for_writing(sender_id, (DeviceType)parent_type);
                    if(fd_parent != -1) {
                        char msg[32];
                        snprintf(msg, sizeof(msg), "MSG %d", hub->id);
                        ipc_send_message(fd_parent, msg);
                        close(fd_parent);
                    }
                } else {
                    char message[MAX_MSG_LEN];
                    int offset = 0;
                    offset += snprintf(message + offset, sizeof(message) - offset, "Hub %d Switch command cascaded to %d children.\n", hub->id, hub->num_children);
                    for(int i=0; i<hub->num_children; i++){
                        offset += snprintf(message + offset, sizeof(message) - offset,
                        "Device %d switch %s.\n", hub->children[i].id, pos);
                    }
                    ipc_send_controller(STATUS_OK, message);
                }
            }
            //INFO
            else if (strncmp(buffer, CMD_INFO, strlen(CMD_INFO)) == 0) {

                char message[MAX_MSG_LEN];

                int offset = 0;

                offset += snprintf(message + offset, sizeof(message) - offset,
                "\n-------- Hub Details -------\n"
                "ID: %d\n", 
                hub->id );
    
                if (hub->parent_id == -1) {
                    offset += snprintf(message + offset, sizeof(message) - offset, "Linked to Hub: no\n");
                } else {
                    offset += snprintf(message + offset, sizeof(message) - offset, "Linked to Hub ID: %d\n", hub->parent_id);
                }

                offset += snprintf(message + offset, sizeof(message) - offset, "Connected devices count: %d\n", hub->num_children);
    
                if (hub->num_children == 0) {
                    offset += snprintf(message + offset, sizeof(message) - offset, "(No devices linked to this Hub)\n");
                } else {

                    char child_states[MAX_CHILDREN][64];
                    char overall_state[64];
                    get_state(hub,fd_ascolto,child_states,overall_state, sizeof(overall_state));

                    offset += snprintf(message + offset, sizeof(message) - offset, "Overall State: %s\nLinked Devices:\n", overall_state);

                    for (int i = 0; i < hub->num_children; i++) {
                        offset += snprintf(message + offset, sizeof(message) - offset, "  %d) ID: %d | Type: %s | State: %s\n", 
                               i + 1, hub->children[i].id, get_device_type_name(hub->children[i].type), child_states[i]);
                    }
                }
                offset += snprintf(message + offset, sizeof(message) - offset, "----------------------------\n");

                ipc_send_controller(STATUS_OK,message);

            } 
            //Mirror
            else if (strncmp(buffer, CMD_MIRROR, strlen(CMD_MIRROR)) == 0) {
                int p_sender_id;
                int p_sender_type;
                if (sscanf(buffer, "%*s %d %d", &p_sender_id, &p_sender_type) == 2) {
                    char child_states[MAX_CHILDREN][64];
                    char overall_state[64];
                    get_state(hub, fd_ascolto, child_states, overall_state, sizeof(overall_state));
                    
                    int fd_parent = ipc_open_for_writing(p_sender_id, p_sender_type);
                    if (fd_parent != -1) {
                        char resp[MAX_MSG_LEN];
                        snprintf(resp, sizeof(resp), "%s %d %s", CMD_MIRROR_RESP, hub->id, overall_state);
                        ipc_send_message(fd_parent, resp);
                        close(fd_parent);
                    }
                }
            } else {
                ipc_send_controller(ERR_INVALID_COMMAND, "Unkown command.");
            }
            

        } else {
            usleep(50000); // 50ms
        }
    }

    close(fd_ascolto);
    exit(0);

}

void create_hub(int id) {
    HubDevice hub = create_hub_struct(id);
    hub_run(&hub);
}


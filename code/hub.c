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
            //unlink
            } else if(strncmp(buffer, CMD_UNLINK_CHILD, strlen(CMD_UNLINK_CHILD)) == 0){
                int child_id;

                if (sscanf(buffer, "%*s %d", &child_id) == 1){

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
                    fprintf(stderr, "Error: invalid LINK_CHILD format. \n");
                }
            //delete        
            } else if(strncmp(buffer, CMD_DELETE, strlen(CMD_DELETE)) == 0) {

                for(int i=0; i < hub->num_children; i++){
                    int child_id = hub->children[i].id;
                    DeviceType child_type = hub->children[i].type;

                    int fd_child = ipc_open_for_writing(child_id,child_type);
                    if(fd_child != -1){
                        ipc_send_message(fd_child, CMD_DELETE);
                        close(fd_child);
                    }
                }

                char msg[MAX_MSG_LEN];
                snprintf(msg,sizeof(msg), "Hub %d and all its children deleted.", hub->id);
                ipc_send_controller(STATUS_OK,msg);

                close(fd_ascolto);
                exit(0);
            }
            //switch
            else if (strncmp(buffer, "SWITCH", 6) == 0) {

                // Propagazione SWITCH a tutti i figli
                for (int i = 0; i < hub->num_children; i++) {
                    int child_id = hub->children[i].id;
                    DeviceType child_type = hub->children[i].type;

                    int fd_child = ipc_open_for_writing(child_id, child_type);
                    if (fd_child != -1) {
                        ipc_send_message(fd_child, buffer);
                        close(fd_child);
                    } 
                }

                char message[MAX_MSG_LEN];
                snprintf(message, sizeof(message), "Hub %d Switch command cascaded to %d children.", hub->id, hub->num_children);
                ipc_send_controller(STATUS_OK, message);
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
                    offset += snprintf(message + offset, sizeof(message) - offset, "Linked to Hub: NO\n");
                } else {
                    offset += snprintf(message + offset, sizeof(message) - offset, "Linked to Hub ID: %d\n", hub->parent_id);
                }

                offset += snprintf(message + offset, sizeof(message) - offset, "Connected devices count: %d\n", hub->num_children);
    
                if (hub->num_children == 0) {
                    offset += snprintf(message + offset, sizeof(message) - offset, "(No devices linked to this Hub)\n");
                } else {

                    for (int i = 0; i < hub->num_children; i++) {

                        //hub chiede lo stato dei figli
                        int child_id = hub->children[i].id;
                        DeviceType child_type = hub->children[i].type;

                        int fd_child = ipc_open_for_writing(child_id,child_type);
                        if(fd_child != -1){
                            char mirror_cmd[32];
                            snprintf(mirror_cmd, sizeof(mirror_cmd), "%s %d", CMD_MIRROR, hub->id);
                            ipc_send_message(fd_child,mirror_cmd);
                            close(fd_child);

                        }
                    }

                    //aspestto una risposta 
                    int response_received = 0;
                    char child_states[MAX_CHILDREN][32];

                    //inizializzo devices a sconosciuto nel caso qualcuno sia crashato
                    for(int i=0; i< hub->num_children; i++){
                        snprintf(child_states[i],sizeof(child_states[i]),"Unknown");
                    }

                    while(response_received < hub->num_children){
                        char mirror_buf[4096];
                        int n = ipc_read_line(fd_ascolto,mirror_buf,sizeof(mirror_buf));

                        if(n > 0){
                            //Cerco tutte le risposte nel buffer
                            char *ptr = mirror_buf;
                            
                            while ((ptr = strstr(ptr, CMD_MIRROR_RESP)) != NULL) {
                                int resp_child_id;
                                char resp_state[32];

                                // estraggo id e stato
                                if(sscanf(ptr, "%*s %d %31s", &resp_child_id, resp_state) == 2){
                                    for(int i = 0; i<hub->num_children; i++){
                                        // Aggiunto il controllo Unknown per non contare due volte per errore
                                        if(hub->children[i].id == resp_child_id){
                                            strncpy(child_states[i], resp_state, sizeof(child_states[i])-1);
                                            response_received++;
                                            break;
                                        }
                                    }
                                }
                                // Spostiamo il puntatore avanti per cercare la prossima risposta
                                ptr += strlen(CMD_MIRROR_RESP);
                            }
                        } else {
                            usleep(50000);
                        }
                    }

                    bool is_active = false;
                    bool is_inactive = false;

                    //scansiono gli stati raccolti per vedere lo stato globale
                    for (int i = 0; i < hub->num_children; i++) {
                        if (strcmp(child_states[i], "On") == 0 || strcmp(child_states[i], "Open") == 0) {
                            is_active = true;
                        } 
                        else if (strcmp(child_states[i], "Off") == 0 || strcmp(child_states[i], "Closed") == 0) {
                            is_inactive = true;
                        }
                    }

                    // Determino lo stato complessivo dell'Hub
                    const char *overall_state = "Unknown";
                    if (is_active && is_inactive) {
                        overall_state = "Manual Override (Discordant)";
                    } else if (is_active) {
                        overall_state = "Active (All On/Open)";
                    } else if (is_inactive) {
                        overall_state = "Inactive (All Off/Closed)";
                    }

                    // Stampiamo lo stato complessivo prima dell'elenco
                    offset += snprintf(message + offset, sizeof(message) - offset, "Overall State: %s\nLinked Devices:\n", overall_state);

                    // Stampo l'elenco dei dispositivi
                    for (int i = 0; i < hub->num_children; i++) {
                        offset += snprintf(message + offset, sizeof(message) - offset, "  %d) ID: %d | Type: %s | State: %s\n", 
                               i + 1, 
                               hub->children[i].id, 
                               get_device_type_name(hub->children[i].type),
                               child_states[i]);
                    }
                }
                offset += snprintf(message + offset, sizeof(message) - offset, "----------------------------\n");

                ipc_send_controller(STATUS_OK,message);

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


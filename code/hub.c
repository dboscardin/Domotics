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

bool hub_add_child(HubDevice *hub, int child_id, DeviceType child_type) {
    if (hub->num_children >= MAX_CHILDREN) {
        return false;
    }

    // Controlliamo se il dispositivo è già stato aggiunto
    for (int i = 0; i < hub->num_children; i++) {
        if (hub->children[i].id == child_id) {
            return true;
        }
    }

    // Colleghiamo il nuovo dispositivo figlio
    hub->children[hub->num_children].id = child_id;
    hub->children[hub->num_children].type = child_type;
    hub->num_children++;

    return true;
}

bool hub_remove_child(HubDevice *hub, int child_id){
    int index = -1;

    for (int i = 0; i < hub->num_children; i++) {
        if (hub->children[i].id == child_id) {
            index = i;
            break;
        }
    }

    if (index == -1) {
        return false;
    }

    // Shift degli elementi verso sinistra per coprire il buco
    for (int i = index; i < hub->num_children - 1; i++) {
        hub->children[i] = hub->children[i + 1];
    }

    hub->num_children--;
    return true;
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
                int child_type_int;

                if (sscanf(buffer, "%*s %d %d", &child_id, &child_type_int) == 2) {
                    if(hub_add_child(hub, child_id, (DeviceType)child_type_int)){
                        char msg[MAX_MSG_LEN];
                        snprintf(msg,sizeof(msg), "Link completed: Device %d is now child of %d.", child_id, hub->id);
                        ipc_send_controller(STATUS_OK,msg);
                    } else {
                        ipc_send_controller(ERR_LINK_FAILED,"Failed to link: max capacity reached.");
                    }
                } else {
                    ipc_send_controller(ERR_INVALID_PARAM, "Invalid link format.");
                }
            //unlink
            } else if(strncmp(buffer, CMD_UNLINK_CHILD, strlen(CMD_UNLINK_CHILD)) == 0){
                int child_id;

                if (sscanf(buffer, "%*s %d", &child_id) == 1){
                    if(hub_remove_child(hub, child_id)){
                        ipc_send_controller(STATUS_OK,"Unlink successful on Hub.");
                    }else {
                        ipc_send_controller(ERR_NOT_FOUND, "Child not found in this Hub.");
                    }
                } else {
                    fprintf(stderr, "Error: invalid LINK_CHILD format. \n");
                }
            //delete        
            } else if(strncmp(buffer, CMD_DELETE, strlen(CMD_DELETE)) == 0) {

                char msg[MAX_MSG_LEN];
                snprintf(msg,sizeof(msg), "Hub %d and all its children deleted.", hub->id);
                ipc_send_controller(STATUS_OK,msg);

                close(fd_ascolto);
                exit(0);
            }
            else if (strncmp(buffer, "SWITCH", 6) == 0) {
                printf("Received SWITCH command. Cascading to %d children...\n", hub->num_children);
                fflush(stdout);

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
                    offset += snprintf(message + offset, sizeof(message) - offset, "  (No devices linked to this Hub)\n");
                } else {
                    offset += snprintf(message + offset, sizeof(message) - offset, "Linked Devices:\n");
                    for (int i = 0; i < hub->num_children; i++) {
                        offset += snprintf(message + offset, sizeof(message) - offset, "  %d) ID: %d | Type: %s\n", 
                               i + 1, 
                               hub->children[i].id, 
                               get_device_type_name(hub->children[i].type));
                    }
                }
                offset += snprintf(message + offset, sizeof(message) - offset, "----------------------------\n");

                ipc_send_controller(STATUS_OK,message);

            }else {
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


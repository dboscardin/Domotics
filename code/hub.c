#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "hub.h"
#include "ipc.h"
#include "device.h"

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

void hub_init(HubDevice *hub, int id) {
    hub->id = id;
    hub->parent_id = -1; // no parent
    hub->num_children = 0;
}

bool hub_add_child(HubDevice *hub, int child_id, DeviceType child_type) {
    if (hub->num_children >= MAX_CHILDREN) {
        fprintf(stderr, "Error: max children limit reached (%d)\n", MAX_CHILDREN);
        return false;
    }

    // Controlliamo se il dispositivo è già stato aggiunto
    for (int i = 0; i < hub->num_children; i++) {
        if (hub->children[i].id == child_id) {
            printf("Child ID %d (%s) is already linked.\n", child_id, get_device_type_name(child_type));
            return true;
        }
    }

    // Colleghiamo il nuovo dispositivo figlio
    hub->children[hub->num_children].id = child_id;
    hub->children[hub->num_children].type = child_type;
    hub->num_children++;

    printf("Linked child device ID: %d (Type: %s)\n", child_id, get_device_type_name(child_type));

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
        printf("Child ID %d not found in this Hub.\n", child_id);
        return false;
    }

    // Shift degli elementi verso sinistra per coprire il buco
    for (int i = index; i < hub->num_children - 1; i++) {
        hub->children[i] = hub->children[i + 1];
    }

    hub->num_children--;
    printf("Unlinked child device ID: %d\n", child_id);
    return true;
}

void create_hub(int id) {
    HubDevice hub;
    hub_init(&hub, id);

    // Creazione FIFO
    ipc_create_fifo(hub.id, DEVICE_HUB);

    // Apertura FIFO ascolto
    int fd_ascolto = ipc_open_for_listening(hub.id, DEVICE_HUB);

    if (fd_ascolto == -1) {
        fprintf(stderr, "Error: it was not possible to open the listening FIFO.\n");
        exit(1);
    }

    // Ricezione messaggi
    char buffer[256];
    while (1) {
        // Lettura FIFO
        int bytes_letti = ipc_read_line(fd_ascolto, buffer, sizeof(buffer));

        if (bytes_letti > 0) {
            printf("Message received: '%s'\n", buffer);
            //link
            if (strncmp(buffer, "LINK_CHILD", 10) == 0) {
                int child_id;
                int child_type_int;

                if (sscanf(buffer, "LINK_CHILD %d %d", &child_id, &child_type_int) == 2) {
                    hub_add_child(&hub, child_id, (DeviceType)child_type_int);
                } else {
                    fprintf(stderr, "Error: invalid LINK_CHILD format.\n");
                }
            //unlink
            } else if(strncmp(buffer, "UNLINK_CHILD", 12) == 0){
                int child_id;

                if (sscanf(buffer, "UNLINK_CHILD %d", &child_id) == 1){
                    hub_remove_child(&hub, child_id);
                } else {
                    fprintf(stderr, "Error: invalid LINK_CHILD format. \n");
                }
            //delete        
            } else if(strncmp(buffer, "DELETE",6) == 0){

                printf("Received DELETE command. Cascading to %d children...\n", hub.num_children);
                
                // Propagazione DELETE a tutti i figli
                for (int i = 0; i < hub.num_children; i++) {
                    int child_id = hub.children[i].id;
                    DeviceType child_type = hub.children[i].type;

                    int fd_child = ipc_open_for_writing(child_id, child_type);
                    if (fd_child != -1) {
                        ipc_send_message(fd_child, "DELETE");
                        close(fd_child);
                    }
                }
                close(fd_ascolto);
                printf("Terminating process.\n");
                exit(0);
            }

        } else {
            usleep(50000); // 50ms
        }
    }

    close(fd_ascolto);
    exit(0);
}

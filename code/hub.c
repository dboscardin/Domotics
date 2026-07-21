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

void create_hub(int id) {
    HubDevice my_hub;
    hub_init(&my_hub, id);

    // Creazione FIFO
    ipc_create_fifo(my_hub.id, DEVICE_HUB);

    // Apertura FIFO ascolto
    int fd_ascolto = ipc_open_for_listening(my_hub.id, DEVICE_HUB);

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

            if (strncmp(buffer, "LINK_CHILD", 10) == 0) {
                int child_id;
                int child_type_int;

                if (sscanf(buffer, "LINK_CHILD %d %d", &child_id, &child_type_int) == 2) {
                    hub_add_child(&my_hub, child_id, (DeviceType)child_type_int);
                } else {
                    fprintf(stderr, "Error: invalid LINK_CHILD format.\n");
                }
            }
        } else {
            usleep(50000); // 50ms
        }
    }

    close(fd_ascolto);
    exit(0);
}
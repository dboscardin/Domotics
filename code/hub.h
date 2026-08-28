#ifndef HUB_H
#define HUB_H

#include <stdbool.h>
#include <stddef.h>
#include "device.h"

// Maximum child device capacity
#define MAX_CHILDREN 50

// Hub structure
typedef struct {
    int id;
    int parent_id;
    ChildDevice children[MAX_CHILDREN];
    int num_children;
} HubDevice;

// Initializes the internal fields of the Hub struct
HubDevice create_hub_struct(int id);

// Adds a child device to the hub
int hub_add_child(HubDevice *hub, int child_id, DeviceType type);

// Removes a child device from the hub
int hub_remove_child(HubDevice *hub, int child_id);

void hub_run(HubDevice *hub);

// Hub creation
void create_hub(int id);

#endif
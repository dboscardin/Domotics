#ifndef HUB_H
#define HUB_H

#include <stdbool.h>
#include <stddef.h>
#include "device.h"

#define MAX_CHILDREN 50

// Struttura dell'Hub
typedef struct {
    int id;
    int parent_id;
    ChildDevice children[MAX_CHILDREN];
    int num_children;
} HubDevice;

// Inizializza le variabili interne della struct Hub
HubDevice create_hub_struct(int id);

//aggiunge un figlio all'hub
bool hub_add_child(HubDevice *hub, int child_id, DeviceType type);

//rimuove un figlio dall'hub
bool hub_remove_child(HubDevice *hub, int child_id);

void hub_run(HubDevice *hub);

//Creazione hub
void create_hub(int id);

#endif
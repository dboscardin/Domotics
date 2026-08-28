#ifndef FRIDGE_H
#define FRIDGE_H

#include <stdbool.h>

typedef struct {
    int id;
    int parent_id;
    bool is_open;                   // Stato dello sportello
    long time;                      // Tempo totale di apertura cumulato
    int delay;                      // Tempo dopo il quale il frigo si chiude da solo
    int perc;                       // Percentuale di riempimento
    int temp;                       // Temperatura interna attuale
    int thermostat;                 // Temperatura target impostata
    struct timespec active_since;   /// Timestamp dell'ultima apertura per il tracking
    bool tracking;                  // true se lo sportello è aperto
} Fridge;

Fridge create_fridge_struct(int id);

void fridge_run(Fridge *fridge);

void create_fridge(int id);

#endif
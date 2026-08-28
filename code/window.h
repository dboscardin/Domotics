#ifndef WINDOW_H
#define WINDOW_H

#include <stdbool.h>

typedef struct {
    int id;
    int parent_id;
    bool is_open;                   // Stato della finestra
    long time;                      // Tempo totale cumulato di apertura
    struct timespec active_since;   // Timestamp dell'ultima apertura per il tracciamento
    bool tracking;                  // Flag che indica se il tempo è attualmente tracciato
} Window;

Window create_window_struct(int id);

void window_run(Window *window);

void create_window(int id);

#endif
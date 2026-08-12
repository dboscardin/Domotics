#ifndef WINDOW_H
#define WINDOW_H

#include <stdbool.h>

typedef struct {
    int id;
    int parent_id;
    bool is_open;
    long time;
    struct timespec active_since;   // timestamp di quando è iniziato lo stato attivo
    bool tracking;                  // true se il timer è attivo
} Window;

Window create_window_struct(int id);

void window_run(Window *window);

void create_window(int id);

#endif
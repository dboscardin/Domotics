#ifndef WINDOW_H
#define WINDOW_H

#include <stdbool.h>

typedef struct {
    int id;
    int parent_id;
    bool is_open;                   // Window open state
    long time;                      // Total accumulated open time
    struct timespec active_since;   // Timestamp of the last opening for tracking
    bool tracking;                  // Flag indicating if time is currently being tracked
} Window;

Window create_window_struct(int id);

void window_run(Window *window);

void create_window(int id);

#endif
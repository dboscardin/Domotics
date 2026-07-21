#ifndef WINDOW_H
#define WINDOW_H

#include <stdbool.h>

typedef struct {
    int id;
    bool is_open;
    int time;
} Window;

Window create_window_struct(int id);

void window_run(Window *window);

void create_window(int id);

#endif
#ifndef WINDOW_H
#define WINDOW_H

#include <stdbool.h>

typedef struct {
    int id;
    bool is_open;
    int time;
} Window;

void create_window(int id);

#endif
#include <stdio.h>
#include <unistd.h>
#include "window.h"

void create_window(int id) {
    Window window = {
        .id = id,
        .is_open = false,
        .time = 0
    };
}
#include <stdio.h>
#include <unistd.h>
#include "window.h"

#define BUFFER_SIZE 50

Window create_window(int id) {
    Window window = {
        .id = id,
        .is_open = false,
        .time = 0
    };
} 

void window_run(Window *window) {
    ipc_create_fifo(window->id, 4);
    int fd = ipc_open_for_listening(window->id, 4);
    char buffer[BUFFER_SIZE];
    while(1) {
        ipc_read_line(fd, buffer, sizeof(buffer));
    }
}
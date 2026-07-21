#include <stdio.h>
#include <unistd.h>
#include "window.h"
#include "ipc.h"

#define BUFFER_SIZE 50

Window create_window_struct(int id) {
    Window window = {
        .id = id,
        .is_open = false,
        .time = 0
    };
    return window;
} 

void window_run(Window *window) {
    int fd = ipc_open_for_listening(window->id, DEVICE_WINDOW);
    char buffer[BUFFER_SIZE];
    while(1) {
        int bytes = ipc_read_line(fd, buffer, sizeof(buffer));
        if (bytes > 0) {
        // messaggio da fifo
        } else {
        usleep(50000); // il processo consuma meno risorse
        }
    }
    close(fd);
}

void create_window(int id) {
    Window w = create_window_struct(id);
    window_run(&w);
    
}
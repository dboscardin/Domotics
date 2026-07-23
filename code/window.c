#include <stdio.h>
#include <unistd.h>
#include "window.h"
#include "ipc.h"
#include "device.h"

#define BUFFER_SIZE 50

Window create_window(int id) {
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
        ipc_read_line(fd, buffer, sizeof(buffer));
    }
}
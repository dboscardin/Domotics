#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "window.h"
#include "ipc.h"
#include "device.h"

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

    srand(time(NULL) ^ getpid());

    int fd = ipc_open_for_listening(window->id, DEVICE_WINDOW);
    char buffer[BUFFER_SIZE];
    while(1) {
        int bytes = ipc_read_line(fd, buffer, sizeof(buffer));
        if (bytes > 0) {

            //delay
            ipc_simulate_delay();

            printf("Message recevied: '%s'\n",buffer);

            //delete
            if (strncmp(buffer, "DELETE",6) == 0){
                printf("Closed Window ID:%d\n", window->id);
                fflush(stdout);
                close(fd);
                exit(0);
            }
            else if(strncmp(buffer , "INFO", 4) == 0){
                printf("------- Window Details -----\n");
                printf("ID: %d\n", window->id);
                printf("State: %s\n", window->is_open ? "Open" : "Closed");
                printf("Time left open: %d s\n", window->time);
                printf("----------------------------\n\n");
                fflush(stdout); 
            }
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
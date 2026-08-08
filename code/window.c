#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "window.h"
#include "ipc.h"
#include "device.h"
#include "protocol.h"

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

            //delete
            if (strncmp(buffer, CMD_DELETE, strlen(CMD_DELETE)) == 0){
                ipc_send_controller(STATUS_OK, "Device deleted.");
                close(fd);
                exit(0);
            }
            //switch
            //TODO sistemare con fifo controller
            else if(strncmp(buffer, "SWITCH", 6) == 0) {
                char label[32], pos[32];
                sscanf(buffer, "SWITCH %s %s", label, pos);

                if(strcmp(label, "is_open") == 0) {
                    window->is_open = (strcmp(pos, "on") == 0);
                }
                if(strcmp(label, "time") == 0) {
                    window->time = atoi(pos);
                }
                printf("[Window %d] %s set to: %s\n", window->id, label, pos);
                fflush(stdout);
            }
            //info
            else if(strncmp(buffer , CMD_INFO, strlen(CMD_INFO)) == 0){
                char message[MAX_MSG_LEN];

                snprintf(message,sizeof(message),
                "\n------- Window Details -----\n"
                "ID: %d\n"
                "State: %s\n" 
                "Time left open: %d s\n"
                "----------------------------\n",
                window->id,window->is_open ? "Open" : "Closed",window->time );
                ipc_send_controller(STATUS_OK, message);
            }else {
                ipc_send_controller(ERR_INVALID_COMMAND, "Unkown command.");
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
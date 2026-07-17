#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h> 
#include "bulb.h"
#include "ipc.h"
#define PERMS 0666

Bulb create_bulb(int id) {
    Bulb bulb = {
        .id = id,
        .power = false,
        .time = 0
    };

    return bulb;
}
int ipc_create_fifo_bulb(int id) {
    char path_name[20];
    sprintf(path_name, "/tmp/bulb%d", id);
    int result = mkfifo(path_name, PERMS);
    if(result == -1) perror("mkfifo");
    return result;
}

int ipc_open_for_listening_bulb(int id) {
    char path_name[20];
    sprintf(path_name, "/tmp/bulb%d", id);
    int fd = open(path_name,  O_RDONLY);
    if(fd == -1) perror("open");
    
    return fd;
}

int ipc_read_line_bulb(int fd, char *buffer, size_t size) {
    ssize_t n = read(fd, buffer, size - 1);
    if (n == -1) perror("read");
    else buffer[n] = '\0'; 
    
    return n;
}

int ipc_send_message_bulb(int fd, const char *message) {
    //es. (fd, "Bulb open", 14)
    ssize_t n = write(fd, message, strlen(message));
    if (n == -1) perror("write");

    return n;
}
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h> 
#include "ipc.h"
#include "device.h"
#define PERMS 0666

const char *names[] = {"controller", "hub", "timer", "bulb", "window", "fridge"};

int ipc_create_fifo(int id, DeviceType type) {
    char path_name[64];
    sprintf(path_name, "/tmp/domotica_%s_%d", names[type], id);
    int result = mkfifo(path_name, PERMS);
    if(result == -1) perror("mkfifo");
    return result;
}

int ipc_open_for_listening(int id, DeviceType type) {
    char path_name[64];
    sprintf(path_name, "/tmp/domotica_%s_%d", names[type], id);
    int fd = open(path_name,  O_RDWR | O_NONBLOCK);
    if(fd == -1) perror("open");
    
    return fd;
}

int ipc_open_for_writing(int id, DeviceType type) {
    char path_name[64];
    sprintf(path_name, "/tmp/domotica_%s_%d", names[type], id);
    int fd = open(path_name,  O_WRONLY);
    if(fd == -1) perror("open");

    return fd;
}

int ipc_read_line(int fd, char *buffer, size_t size) {
    ssize_t n = read(fd, buffer, size - 1);
    
    if (n > 0) {
        buffer[n] = '\0';
        // Rimuove il carattere '\n' o '\r' finale
        buffer[strcspn(buffer, "\r\n")] = '\0'; 
    } else {
        //se n <= 0 azzeriamo il buffer
        buffer[0] = '\0';
    }
    
    return n;
}

int ipc_send_message(int fd, const char *message) {
    //es. (fd, "Bulb open", 14)
    ssize_t n = write(fd, message, strlen(message));
    if (n == -1) perror("write");

    return n;
}
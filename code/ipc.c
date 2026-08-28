#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h> 
#include <stdlib.h>
#include <time.h>
#include "ipc.h"
#include "device.h"
#include "protocol.h"

#define PERMS 0666

//Array per convertire l'enum DeviceType in stringa
const char *names[] = {"controller", "hub", "timer", "bulb", "window", "fridge"};

int ipc_create_fifo(int id, DeviceType type) {
    char path_name[64];
    snprintf(path_name, sizeof(path_name), FIFO_PATH_FMT, names[type], id);

    //elimina eventuali vecchie fifo 
    unlink(path_name);

    int result = mkfifo(path_name, PERMS);
    if(result == -1) perror("mkfifo");
    return result;
}

int ipc_open_for_listening(int id, DeviceType type) {
    char path_name[64];
    snprintf(path_name, sizeof(path_name), FIFO_PATH_FMT, names[type], id);
    //apro la fifo in O_RDWR per evitare di ricevere continuamente EOF quando non ci sono processi collegati
    // O_NONBLOCK permette al ciclo while(1) dei device di non bloccarsi
    int fd = open(path_name,  O_RDWR | O_NONBLOCK);
    if(fd == -1) perror("open");
    
    return fd;
}

int ipc_open_for_writing(int id, DeviceType type) {
    char path_name[64];
    snprintf(path_name, sizeof(path_name), FIFO_PATH_FMT, names[type], id);
    int fd = open(path_name,  O_WRONLY | O_NONBLOCK);
    if(fd == -1) perror("open");

    return fd;
}

int ipc_read_line(int fd, char *buffer, size_t size) {
    // legge dalla FIFO
    ssize_t n = read(fd, buffer, size - 1);
    
    if (n > 0) {
        buffer[n] = '\0';
        // Rimuove il carattere '\n' o '\r' finale
        buffer[strcspn(buffer, "\r\n")] = '\0'; 
    } else {
        // Se non è stato letto nulla azzeriamo il buffer
        buffer[0] = '\0';
    }
    
    return n;
}

int ipc_send_message(int fd, const char *message) {
    // Invia la stringa direttamente alla FIFO
    ssize_t n = write(fd, message, strlen(message));
    if (n == -1) perror("write");

    return n;
}

void ipc_remove_fifo(int id, DeviceType type){
    char path_name[64];
    snprintf(path_name, sizeof(path_name), FIFO_PATH_FMT, names[type], id);
    unlink(path_name);
}

// Genera un ritardo casuale da 1 a 3 secondi
void ipc_simulate_delay(void){
    int seconds = (rand() % 3) + 1;
    sleep(seconds);
}

// Notifica il controller tarmite FIFO principale
void ipc_send_controller(int status_code, const char *message){
    int fd = open(FIFO_CONTROLLER, O_WRONLY | O_NONBLOCK);

    if(fd != -1){
        char buffer[MAX_MSG_LEN];
        
        if(message != NULL && strlen(message) > 0 ){
            snprintf(buffer,sizeof(buffer), RESP_FORMAT, status_code,message);
        } else {
            snprintf(buffer,sizeof(buffer), RESP_FORMAT_NO_PAYLOAD, status_code);
        }

        ssize_t written = write(fd, buffer, strlen(buffer));
        if (written == -1) {
            perror("ipc_send_controller: write");
        }
        close(fd);
    }
}

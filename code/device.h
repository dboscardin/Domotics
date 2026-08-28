// ciò che è comune a tutti i device
#ifndef DEVICE_H
#define DEVICE_H

#include <sys/types.h>
#include <stdbool.h>

// tipologia del dispositivo
typedef enum {
    DEVICE_CONTROLLER,
    DEVICE_HUB,
    DEVICE_TIMER,
    DEVICE_BULB,
    DEVICE_WINDOW,
    DEVICE_FRIDGE
} DeviceType;

typedef struct {
    int id;
    pid_t pid;
    DeviceType type;
    int fifo_fd;
    int parent_id;
} DeviceInfo;

//per hub e timer
typedef struct {
    int id;
    DeviceType type;
} ChildDevice;

// Estrae il nuovo parent_id dal comando IPC e aggiorna la variabile del dispositivo
void handle_set_parent(int *parent_id, const char *buffer);

// Formatta il parent_id in una stringa leggibile
void format_parent_string(int parent_id, char *parent, size_t size);

// Calcola i secondi trascorsi da un dato timestamp
long compute_elapsed_seconds(struct timespec *active_since);

#endif
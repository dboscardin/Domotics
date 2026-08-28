#ifndef IPC_H
#define IPC_H
#include "device.h"

int ipc_create_fifo(int id, DeviceType type);

int ipc_open_for_listening(int id, DeviceType type);

int ipc_open_for_writing(int id, DeviceType type);

int ipc_read_line(int fd, char *buffer, size_t size);

int ipc_send_message(int fd, const char *message);

void ipc_remove_fifo(int id, DeviceType type);

void ipc_simulate_delay(void);

// Funzione per inviare comodamente codice di stato al Controller
void ipc_send_controller(int status_code, const char *message);

#endif

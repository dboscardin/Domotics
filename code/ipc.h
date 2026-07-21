#ifndef IPC_H
#define IPC_H
#include "device.h"

int ipc_create_fifo(int id, DeviceType type);

int ipc_open_for_listening(int id, DeviceType type);

int ipc_open_for_writing(int id, DeviceType type);

int ipc_read_line(int fd, char *buffer, size_t size);

int ipc_send_message(int id, const char *message);

#endif

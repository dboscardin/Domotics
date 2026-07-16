//funzioni condivise tra controller e device


//crea fifo se non esiste già
int ipc_create_fifo(int id);

int ipc_open_for_listening(int id);

int ipc_read_line(int fd, char *buffer, size_t size);

int ipc_send_message(int id, const char *message);
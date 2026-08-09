#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "controller.h"
#include "device.h"
#include "bulb.h"
#include "window.h"
#include "fridge.h"
#include "ipc.h"
#include "hub.h"
#include "protocol.h"
#include "timer.h"

#define MAX_CMD_LEN 50
#define MAX_DEVICES 50
#define MAX_TOKENS 10

//veriabile globale per controllare il ciclo di un thread
static volatile int running = 1;
static pthread_mutex_t mutex;
static pthread_cond_t sync_cond;
static volatile bool response_received = false;

static DeviceInfo devices[MAX_DEVICES];
static int device_count = 0;    //conta dispositivi attuali
static int curr_id = 0;         //assegna un id che non decrementa all'eliminazione
//devo averne due per evitare conflitti causa eliminazione

static void devices_list(void);
static void add_device(char* device);
static int parse_id(const char *charId);
static int find_device_by_id(int id);
static void link_devices(int child_id, int hub_id);
static void remove_device(int id);
static bool switch_check(char *tokens[], int count);
static void switch_device(char *tokens[]);
static void device_info(int id);
static void commands(void);
static void cleanup_all_devices(void);
static void handle_sigint(int sig);
static void unlink_device(int child_id,int hub_id);
static void unlink_children_from_timer(int parent_id);
static void remove_device_from_array(int id);
static void remove_children_from_hub(int parent_id);

static const char *device_type_to_string(DeviceType type) {
    switch (type) {
        case DEVICE_BULB:       return "Bulb";
        case DEVICE_WINDOW:     return "Window";
        case DEVICE_FRIDGE:     return "Fridge";
        case DEVICE_CONTROLLER: return "Controller";
        case DEVICE_HUB:        return "Hub";
        case DEVICE_TIMER:      return "Timer";
        default:                return "Unknown";
    }
}

// Funzione per terminare tutti i processi figli
static void cleanup_all_devices(void) {
    for (int i = 0; i < device_count; i++) {
        if (devices[i].fifo_fd != -1) {
            close(devices[i].fifo_fd);
        }
        ipc_remove_fifo(devices[i].id, devices[i].type);
        kill(devices[i].pid, SIGTERM);
        waitpid(devices[i].pid, NULL, 0);
    }
    device_count = 0;
    unlink(FIFO_CONTROLLER);
    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&sync_cond);
}

// Gestione della chiusura tramite Ctrl+C
static void handle_sigint(int sig) {
    (void)sig;
    printf("\nInterrupted. Terminating all background devices...\n");
    cleanup_all_devices();
    exit(0);
}

// Handler del segnale SIGCHLD 
static void handle_sigchld(int sig) {
    (void)sig;
    int status;
    pid_t pid;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        for (int i = 0; i < device_count; i++) {
            if (devices[i].pid == pid) {
                if (devices[i].fifo_fd != -1) {
                    close(devices[i].fifo_fd);
                    devices[i].fifo_fd = -1;
                }
                ipc_remove_fifo(devices[i].id, devices[i].type);
                
                // Shift dell'array per rimuovere il device eliminato dallo script
                for (int j = i; j < device_count - 1; j++) {
                    devices[j] = devices[j + 1];
                }
                device_count--;
                break;
            }
        }
    }
}

static int read_line(char *buffer, size_t size) {
   while (1) {
        if (fgets(buffer, size, stdin) == NULL) {
            if (errno == EINTR) {
                clearerr(stdin);
                errno = 0;
                continue;
            }
            return 0;
        }
        break;
    }

    buffer[strcspn(buffer, "\n")] = '\0';
    return 1;
}

static void devices_list(void) {
    if(device_count == 0)
        printf("No devices yet\n");
    else {
        for (int i = 0; i < device_count; i++)
        {
            printf("%d --> Id=%d, Pid=%d, Type=%s, ", (i + 1), devices[i].id, devices[i].pid, device_type_to_string(devices[i].type));
            if(devices[i].parent_id == -1){
                printf("Linked: NO\n");
            } else {
                printf("Linked to ID: %d\n", devices[i].parent_id);
            }
        }
        printf("\n");
    }
}

static void add_device(char* device) {
    DeviceType type; 

    if(strcmp(device, "bulb") == 0) {
        type = DEVICE_BULB;
    }
    else if(strcmp(device, "window") == 0) {
        type = DEVICE_WINDOW;
    }
    else if(strcmp(device, "fridge") == 0) {
        type = DEVICE_FRIDGE;
    }
    else if(strcmp(device, "hub") == 0) {
        type = DEVICE_HUB;
    }
    else if (strcmp(device, "timer") == 0) {
        type = DEVICE_TIMER;

    }
    else {
        printf("Invalid device type.\n");
        return;
    }

    if(device_count >= MAX_DEVICES) {
        printf("You reached the limit of devices.\n");
        return;
    }

    if(ipc_create_fifo(curr_id, type) == -1) {
        printf("FIFO creation failed.\n");
        return;
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("Error during fork.");
        return;
    }
    
    if(pid == 0) {
        signal(SIGINT,SIG_DFL);
        close(STDIN_FILENO); // Chiude l'input tastiera per il figlio
        switch (type) {
            case DEVICE_BULB:
                create_bulb(curr_id);
                break;
            case DEVICE_WINDOW:
                create_window(curr_id);
                break;
            case DEVICE_FRIDGE:
                create_fridge(curr_id);
                break;
            case DEVICE_HUB:
                create_hub(curr_id);
                break;
            case DEVICE_TIMER:
                create_timer(curr_id);
                break;
            default:
                _exit(1);
        }
        _exit(0);
    }

    devices[device_count].id = curr_id;
    devices[device_count].pid = pid;
    devices[device_count].type = type;
    devices[device_count].parent_id = -1;
    devices[device_count].fifo_fd = -1;

    usleep(50000); //50ms

    printf("%s", device_type_to_string(type));    
    printf(" created successfully!\nid=%d, pid=%d\n", curr_id, pid);

    device_count++;
    curr_id++;

}

static int parse_id(const char *charId) {
    char *endptr;

    long val = strtol(charId, &endptr, 10);

    if(endptr == charId || *endptr != '\0' || val < 0) {
        printf("Invalid ID.\n");
        return -1;
    }
    return (int)val;
}

static int find_device_by_id(int id) {
   for(int i = 0; i < device_count; i++) {
        if(devices[i].id == id)
            return i;
    }
    return -1;
}

//ritorna true se collegare child_id sotto hub_id creerebbe un ciclo (se child_id è già antenato di hub_id)
static bool creates_cycle(int child_id, int hub_id) {
    int curr_id = hub_id;
    int steps = 0;

    while(curr_id != -1) {
        if(curr_id == child_id) {
            return true;
        }

        int idx = find_device_by_id(curr_id);
        if(idx == -1) {
            //genitor non nell'array --> controller
            break;
        }

        curr_id = devices[idx].parent_id;

        steps++;
        if(steps > device_count) {
            break;
        }
    }
    return false;
}
  
static void link_devices(int child_id, int hub_id) {
    if(child_id == hub_id) {
        printf("Error: you can't link a device to itself.\n");
        return;
    }
    int child_idx = find_device_by_id(child_id);
    int hub_idx = find_device_by_id(hub_id);

    if(creates_cycle(child_id, hub_id)) {
        printf("Error: this link would create a cycle in the hierarchy.\n\n");
        return;
    }

    if (child_idx == -1) {
        printf("Error: child device with ID %d does not exist.\n\n", child_id);
        return;
    }

    if (hub_idx == -1) {
        printf("Error: Hub with ID %d does not exist.\n\n", hub_id);
        return;
    }

    if (devices[hub_idx].type != DEVICE_HUB && devices[hub_idx].type != DEVICE_TIMER) {
        printf("Error: device ID %d is not a Hub or Timer.\n\n", hub_id);
        return;
    }

    if(creates_cycle(child_id, hub_id)) {
        printf("Error: this link would create a cycle in the hierarchy.\n\n");
        return;
    }

    //invia messaggio al figlio
    int fd_child = ipc_open_for_writing(child_id, devices[child_idx].type);
    if (fd_child != -1) {
        char msg_child[64];
        snprintf(msg_child, sizeof(msg_child), "%s %d %d",CMD_SET_PARENT , child_id, hub_id);

        ipc_send_message(fd_child, msg_child);
        close(fd_child);

    } else {
        printf("Error: failed to connect to child %d FIFO.\n\n", child_id);
    }

    //invia messaggio al padre
    int fd_parent = ipc_open_for_writing(hub_id, devices[hub_idx].type);
    if (fd_parent != -1) {
        char msg_parent[64];
        snprintf(msg_parent, sizeof(msg_parent), "%s %d %d", CMD_LINK_CHILD,child_id, devices[child_idx].type);

        pthread_mutex_lock(&mutex);
        response_received = false;

        ipc_send_message(fd_parent, msg_parent);
        close(fd_parent);

        while(!response_received){
            pthread_cond_wait(&sync_cond,&mutex);
        }

        pthread_mutex_unlock(&mutex);

        devices[child_idx].parent_id = hub_id;

    } else {
        printf("Error: failed to connect to parent %d FIFO.\n\n", hub_id);
    }

}

static void unlink_device(int child_id,int hub_id){
    int child_idx = find_device_by_id(child_id);
    int hub_idx = find_device_by_id(hub_id);

    if (child_idx == -1) {
        printf("Error: child device with ID %d does not exist.\n\n", child_id);
        return;
    }

    if (hub_idx == -1) {
        printf("Error: Hub with ID %d does not exist.\n\n", hub_id);
        return;
    }

    if (devices[hub_idx].type != DEVICE_HUB && devices[hub_idx].type != DEVICE_TIMER) {
        printf("Error: device ID %d is not a Hub or Timer.\n\n", hub_id);
        return;
    }

    char msg[64];
    snprintf(msg, sizeof(msg), "%s %d",CMD_UNLINK_CHILD, child_id);
    int fd = ipc_open_for_writing(hub_id, devices[hub_idx].type);
    if (fd != -1) {

        pthread_mutex_lock(&mutex);
        response_received = false;

        ipc_send_message(fd, msg);
        close(fd);

        while(!response_received){
            pthread_cond_wait(&sync_cond,&mutex);
        }

        pthread_mutex_unlock(&mutex);

        devices[child_idx].parent_id = -1;

    } else {
        printf("Error: failed to connect to Hub %d FIFO.\n\n", hub_id);
    }
}

static void unlink_children_from_timer(int parent_id){
    for(int i=0; i<device_count; i++){
        if(devices[i].parent_id == parent_id){
            devices[i].parent_id=-1;
        }
    }
}

//rimuove il device dall'array devices
static void remove_device_from_array(int id) {
    int index = find_device_by_id(id);
    if (index == -1) return;

    if (devices[index].fifo_fd != -1) {
        close(devices[index].fifo_fd);
    }

    ipc_remove_fifo(devices[index].id, devices[index].type);

    for (int i = index; i < device_count - 1; i++) {
        devices[i] = devices[i + 1];
    }
    device_count--;
}

// Rimuove ricorsivamente i figli associati a un hub
static void remove_children_from_hub(int parent_id) {
    for (int i = device_count - 1; i >= 0; i--) {
        if (devices[i].parent_id == parent_id) {
            int child_id = devices[i].id;
            DeviceType child_type = devices[i].type;
            pid_t child_pid = devices[i].pid;

            // Se il figlio è un Hub, elimina ricorsivamente i suoi sotto-figli
            if (child_type == DEVICE_HUB) {
                remove_children_from_hub(child_id);
            } 
            // Se il figlio è un Timer, svincola solo i suoi figli senza distruggerli
            else if (child_type == DEVICE_TIMER) {
                unlink_children_from_timer(child_id);
            }

            int fd = ipc_open_for_writing(child_id, child_type);
            if (fd != -1) {

                pthread_mutex_lock(&mutex);
                response_received = false;

                ipc_send_message(fd, CMD_DELETE);
                close(fd);

                while(!response_received){
                    pthread_cond_wait(&sync_cond,&mutex);
                }

                pthread_mutex_unlock(&mutex);
            } else {
                kill(child_pid, SIGKILL);
            }
            
            waitpid(child_pid, NULL, 0);

            // Rimuove l'elemento dall'array devices del Controller
            remove_device_from_array(child_id);
        }
    }
}

static void remove_device(int id) {
    int index = find_device_by_id(id);
    if (index == -1) {
        printf("No device with this Id.\n\n");
        return;
    }

    //evita race condition
    sigset_t mask, oldmask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGCHLD);
    sigprocmask(SIG_BLOCK, &mask, &oldmask);

    if (devices[index].parent_id != -1) {
        unlink_device(id, devices[index].parent_id);
    }

    DeviceType type = devices[index].type;
    pid_t pid = devices[index].pid;

    //HUB vengono eliminati anche i figli
    if (type == DEVICE_HUB) {
        remove_children_from_hub(id);
    } 
    //timer non elimina i figli
    else if (type == DEVICE_TIMER) {
        unlink_children_from_timer(id);
    }
        

    // Invio DELETE
    int fd = ipc_open_for_writing(id, type);
    if (fd != -1) {

        pthread_mutex_lock(&mutex);
        response_received = false;

        ipc_send_message(fd, CMD_DELETE);
        close(fd);

        while(!response_received){
            pthread_cond_wait(&sync_cond,&mutex);
        }

        pthread_mutex_unlock(&mutex);
    } else {
        kill(pid, SIGKILL);
    }

    remove_device_from_array(id);
    printf("Device ID: %d is removed\n\n", id);
    fflush(stdout);

    sigprocmask(SIG_SETMASK, &oldmask, NULL);
}
static bool switch_check(char *tokens[], int count) {
    //check array size
    if(count != 4) return false; 
    //check if it's  valid ID
    if(parse_id(tokens[1]) == -1) return false;
    //check if it works on a right attribute 
    struct {
        const char *label;
        bool is_bool;
    } registers[] = {
        {"power", true},
        {"is_open", true},
        {"time", false},
        {"delay", false},
        {"perc", false},
        {"temp", false},
        {"thermostat", false}
    };

    for(size_t i = 0; i < sizeof(registers) / sizeof(registers[0]); i++) {
        if(strcmp(tokens[2], registers[i].label) == 0) {
            if(registers[i].is_bool) {
                return strcmp(tokens[3], "on") == 0 || strcmp(tokens[3], "off") == 0;
            } else {
                char *endptr;
                //tries to convert in int
                strtol(tokens[3], &endptr, 10);
                //se endptr punta alla fine, tutta la stringa era un numero
                return endptr != tokens[3] && *endptr == '\0';
            }
        }
    }
    return false;
}

static void switch_device(char *tokens[]) {
    int id = parse_id(tokens[1]);
    int index = find_device_by_id(id);
    if(index == -1) return;

    char message[MAX_MSG_LEN];
    snprintf(message, sizeof(message), "%s %s %s", CMD_SWITCH, tokens[2], tokens[3]);

    int fd = ipc_open_for_writing(id, devices[index].type);
    if(fd != -1) {

        pthread_mutex_lock(&mutex);
        response_received = false;

        ipc_send_message(fd, message);
        close(fd);

        while (!response_received) {
            pthread_cond_wait(&sync_cond, &mutex);
        }
        pthread_mutex_unlock(&mutex);

    } else {
        printf("Error: Failed to communicate with Device %d.\n", id);
    }
}

static void device_info(int id) {
    int index = find_device_by_id(id);
    if (index == -1){
        printf("Device ID: %d not found\n\n", id);
        return;
    }

    int fd = ipc_open_for_writing(id, devices[index].type);
    if (fd != -1 ){

        pthread_mutex_lock(&mutex);
        response_received = false;

        ipc_send_message(fd, CMD_INFO);
        close(fd);

        while (!response_received) {
            pthread_cond_wait(&sync_cond, &mutex);
        }
        pthread_mutex_unlock(&mutex);
    } else {
        printf("Error: failed to communicate with device ID: %d\n\n ", id);
    }



}

static void commands(void) {
    printf("Commands list:\n");
    printf("list: Lists all devices.\n");
    printf("add <device>: Spawns a new device in the house. (Max 50 devices)\n");
    printf("del <id>: Delete an existing device.\n");
    printf("link <id1> to <id2>: id1 will be controlled by id2.\n");
    printf("unlink <id1> from <id2>: Unlinks id1 from hub id2.\n");
    printf("switch <id> <label> <pos>: Sets the switch label of device id to position pos.\n");
    printf("info <id>: Displays the complete details of the device\n");
    printf("quit: To quit the program.\n");
}

static void *listener_thread(void *arg){

    (void)arg;

    //blocchimo SIGCHLD cosi da non bloccare il thread quando eliminiamo un figlio
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask,SIGCHLD);
    pthread_sigmask(SIG_BLOCK,&mask,NULL);

    //apro la fifo in sola lettura/scrittura per evitare il blocco se nessun processo figlio è collegato
    int controller_fd = open(FIFO_CONTROLLER, O_RDWR);
    if(controller_fd == -1){
        perror("Error opening Controller FIFO");
        pthread_exit(NULL);
    }

    char controller_buf[MAX_MSG_LEN];
    while(running){

        //thread in attesa per consumare meno cpu
        ssize_t n = read(controller_fd, controller_buf, sizeof(controller_buf)-1);

        if(n > 0){
            controller_buf[n] = '\0';
            
            char *message = strchr(controller_buf, ' ');
            if(message != NULL){
                message++;
            } else {
                message = controller_buf;
            }

            //sovrascivo il prompt domotics con il messaggio e lo riscrivo
            printf("%s\n", message);
            fflush(stdout);

            pthread_mutex_lock(&mutex);
            response_received = true;
            pthread_cond_signal(&sync_cond);
            pthread_mutex_unlock(&mutex);

        }

    }

    close(controller_fd);
    return NULL;
}


void controller_run(void) {

    //evita la chiusura del controller quando si invia un comando a un device morto
    signal(SIGPIPE, SIG_IGN);

    // Configura SIGCHLD con SA_RESTART per catturare eliminazioni da file .sh
    struct sigaction sa_child;
    sa_child.sa_handler = handle_sigchld;
    sigemptyset(&sa_child.sa_mask);
    sa_child.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &sa_child, NULL);

    //Ctrl+C
    struct sigaction sa;
    sa.sa_handler = handle_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);

    //create fifo controller
    unlink(FIFO_CONTROLLER);
    if(mkfifo(FIFO_CONTROLLER, 0666) == -1){
        perror("Error creating Controller FIFO");
        exit(1);
    }

    pthread_mutex_init(&mutex, NULL);
    pthread_cond_init(&sync_cond, NULL);

    //thread
    pthread_t listener_td;
    if(pthread_create(&listener_td, NULL, listener_thread, NULL) != 0){
        perror("Failed to create listener thread");
        exit(1);
    }


    char buffer[MAX_CMD_LEN];  

    printf("What do you want to do?\n");
    while(running) {

        printf("domotics> ");
        fflush(stdout);


        if (!read_line(buffer, sizeof(buffer))) {
            printf("Exit...\n\n");
            break;
        }

        char *tokens[MAX_TOKENS];
        int count = 0;
        char *currToken = strtok(buffer, " ");

        while(currToken != NULL && count < MAX_TOKENS) {
            tokens[count] = currToken;
            count++;
            currToken = strtok(NULL, " ");
        }

        //evita il crash se l'utente preme invio senza scrivere nulla
        if (count == 0) {
            continue;
        }

        //switch non si può fare perché non funziona con le stringhe (solo numeri e char)
        if(strcmp(tokens[0], "list") == 0) {
            devices_list();
        }
        else if(strcmp(tokens[0], "add") == 0) {
            if(count != 2) {
                printf("Invalid command. Structure should be: add <device>.\n");
            } else {
                add_device(tokens[1]);
            }
        }
        else if(strcmp(tokens[0], "del") == 0) {
            if(count != 2) {
                printf("Invalid command. Structure should be: del <id>. \n");
            }
            else {
                int id = parse_id(tokens[1]);
                if(id != -1) {
                    remove_device(id);
                } else {
                    printf("Invalid id.\n");
                }
            }
        }
        else if(strcmp(tokens[0], "link") == 0) {
            if (count < 4 || strcmp(tokens[2], "to") != 0) {
                printf("Invalid command format. Use: link <id1> to <id2>\n\n");
            } else {
                int child_id = parse_id(tokens[1]);
                int hub_id = parse_id(tokens[3]);

                if (child_id != -1 && hub_id != -1) {
                    link_devices(child_id, hub_id);
                }
            }
        }
        else if(strcmp(tokens[0], "unlink") == 0){
            int child_id = -1, hub_id = -1;

            if (count == 4 && strcmp(tokens[2], "from") == 0) {
                child_id = parse_id(tokens[1]);
                hub_id = parse_id(tokens[3]);
            }
            else {
                printf("Invalid command format. Use: unlink <id1> from <id2>\n");
            }

            if (child_id != -1 && hub_id != -1) {
                unlink_device(child_id, hub_id);
            }
        }
        else if(strcmp(tokens[0], "switch") == 0) {
            if(switch_check(tokens, count)) {
                switch_device(tokens);
            } else {
                printf("Invalid command format. Use  switch <id> <label> <pos>\n");
            }
        }

        else if(strcmp(tokens[0], "info") == 0) {
            if(count < 2) {
                printf("Invalid command. Device id is missing. \n");
            }
            else {
                int id = parse_id(tokens[1]);
                if(id != -1) {
                    device_info(id);
                } else {
                    printf("Invalid id.\n");
                }
            }
        }
        else if(strcmp(tokens[0], "cmds") == 0) {
            commands();
        }
        else if(strcmp(tokens[0], "quit") == 0) {
            printf("Exit...\n\n");

            running = 0;//termina il ciclo nel thread e anche qui

            //per far uscire il thread dalla read bloccante gli si manda un byte fittizio
            int fd = open(FIFO_CONTROLLER, O_WRONLY | O_NONBLOCK);
            if (fd != -1) { 
                write(fd, " ", 1); 
                close(fd); 
            }

            pthread_join(listener_td, NULL);
            cleanup_all_devices();
            return;
        }
        else {
            printf("Invalid command.\n");
        }
        
    }   
}
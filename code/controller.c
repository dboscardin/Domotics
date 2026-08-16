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

static bool controller_state = true;

static DeviceInfo devices[MAX_DEVICES];
static int device_count = 0;    //conta dispositivi attuali
//parte da 1 perchè 0 è riservato al controller
static int curr_id = 1;         //assegna un id che non decrementa all'eliminazione
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
static void remove_device_from_array(int id);
static int count_direct_children(int parent_id);

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

// ritorna numero di device di un padre
static int count_direct_children(int parent_id) {
    int count = 0;
    for (int i = 0; i < device_count; i++) {
        if (devices[i].id != CONTROLLER_ID && devices[i].parent_id == parent_id) {
            count++;
        }
    }
    return count;
}

// Funzione per terminare tutti i processi figli
static void cleanup_all_devices(void) {

    signal(SIGCHLD, SIG_DFL);
    
    for (int i = 0; i < device_count; i++) {
        if (devices[i].id == CONTROLLER_ID) {
            continue;
        }
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
    running = 0;
    // Sblocca il thread in ascolto mandando un byte fittizio
    int fd = open(FIFO_CONTROLLER, O_WRONLY | O_NONBLOCK);
    if (fd != -1) { 
        write(fd, " ", 1); 
        close(fd); 
    }
}

// Handler del segnale SIGCHLD 
static void handle_sigchld(int sig) {
    (void)sig;
    int status;
    pid_t pid;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        for (int i = 0; i < device_count; i++) {
            if (devices[i].id == CONTROLLER_ID) {
                continue; 
            }
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
                if(!running){
                    return 0;
                }
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
    for (int i = 0; i < device_count; i++)
    {
        printf("%d --> Id=%d, Pid=%d, Type=%s, ", (i + 1), devices[i].id, devices[i].pid, device_type_to_string(devices[i].type));
        if(devices[i].parent_id == -1){
            printf("Root service\n");
        } else if(devices[i].parent_id == CONTROLLER_ID){
            printf("Linked to Controller\n");
        } else {
            printf("Linked to ID: %d\n", devices[i].parent_id);
        }
    }
    printf("\n");
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
        signal(SIGINT,SIG_IGN);
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
    devices[device_count].parent_id = CONTROLLER_ID;
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

    if (devices[child_idx].parent_id != -1) {
        printf("Notice: Device %d is already linked to %d. Unlinking...\n", child_id, devices[child_idx].parent_id);
        unlink_device(child_id, devices[child_idx].parent_id);
        link_devices(child_id,hub_id);
        return;
    }

    //invia messaggio al figlio
    int fd_child = ipc_open_for_writing(child_id, devices[child_idx].type);
    if (fd_child != -1) {
        char msg_child[64];
        snprintf(msg_child, sizeof(msg_child), "%s %d",CMD_SET_PARENT, hub_id);

        ipc_send_message(fd_child, msg_child);
        close(fd_child);

    } else {
        printf("Error: failed to connect to child %d FIFO.\n\n", child_id);
    }

    //invia messaggio al padre
    if (hub_id != CONTROLLER_ID) {
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

    if (devices[hub_idx].type != DEVICE_HUB && devices[hub_idx].type != DEVICE_TIMER && devices[hub_idx].type != DEVICE_CONTROLLER) {
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

static void remove_device(int id) {
    if (id == CONTROLLER_ID) {
        printf("Error: the Controller cannot be deleted.\n\n");
        return;
    }
    int index = find_device_by_id(id);
    if (index == -1) {
        printf("No device with this Id.\n\n");
        return;
    }

    // Se il dispositivo era collegato a un genitore
    if (devices[index].parent_id != -1) {
        unlink_device(id, devices[index].parent_id);
    }

    DeviceType type = devices[index].type;
    pid_t pid = devices[index].pid;

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
        //solo se la fifo non è accessibile
        kill(pid, SIGKILL);
    }

    remove_device_from_array(id);
    printf("Device ID: %d is removed\n\n", id);
    fflush(stdout);

    
}
static bool label_valid_for_type(DeviceType type, const char *label) {
    switch(type) {
        case DEVICE_CONTROLLER:
            return strcmp(label, "main") == 0;
        case DEVICE_BULB:
            return strcmp(label, "power") == 0;
        case DEVICE_WINDOW:
            return strcmp(label, "open") == 0 || strcmp(label, "close") == 0;
        case DEVICE_FRIDGE:
            if (strcmp(label, "perc") == 0 || strcmp(label, "thermostat") == 0) {
                printf("Error: '%s' can only be modified manually via bash script.\n", label);
                return false;
            }
            return strcmp(label, "open")       == 0 ||
                strcmp(label, "close")      == 0 ||
                strcmp(label, "delay")      == 0;
        case DEVICE_HUB:
        case DEVICE_TIMER:
            // tutto
            return true;
        default:
            return false;
    }
}

static bool switch_check(char *tokens[], int count) {
    if (count != 4) return false;
    int id = parse_id(tokens[1]);
    if (id == -1) return false;
    int idx = find_device_by_id(id);
    if (idx == -1) return false;

    if (!label_valid_for_type(devices[idx].type, tokens[2])) {
        printf("Error: label '%s' is not valid for device type %s.\n",
            tokens[2], device_type_to_string(devices[idx].type));
        return false;
    }

    struct {
        const char *label;
        bool is_bool;
    } registers[] = {
        {"power",       true},
        {"open",        true},
        {"close",       true},
        {"delay",       false},
        {"begin",       false},
        {"end",         false}
    };

    for(size_t i = 0; i < sizeof(registers) / sizeof(registers[0]); i++) {
        if(strcmp(tokens[2], registers[i].label) == 0) {
            if(registers[i].is_bool) {
                return strcmp(tokens[3], "on") == 0 || strcmp(tokens[3], "off") == 0;
            } else {

                //per il timer accettando le stringhe per gli orari
                if(strcmp(tokens[2], "begin") == 0 || strcmp(tokens[2],"end")== 0){
                    return true;
                }

                char *endptr;
                strtol(tokens[3], &endptr, 10);
                return endptr != tokens[3] && *endptr == '\0';
            }
        }
    }
    return false;
}

static void switch_device(char *tokens[]) {
    int id = parse_id(tokens[1]);

    if (id == CONTROLLER_ID) {
        if (strcmp(tokens[2], "main") != 0) {
            printf("Error: the Controller only supports the 'main' switch.\n\n");
            return;
        }
        controller_state = (strcmp(tokens[3], "on") == 0);
        printf("Controller main switch set to: %s. Cascading to all connected devices...\n\n", controller_state ? "on" : "off");
        
        // Propaga il comando a tutti i figli diretti del Controller
        for (int i = 0; i < device_count; i++) {
            if (devices[i].id != CONTROLLER_ID && devices[i].parent_id == CONTROLLER_ID) {
                int fd_child = ipc_open_for_writing(devices[i].id, devices[i].type);
                if (fd_child != -1) {
                    char cmd[64];
                    const char *out_label = "power";
                    const char *out_pos = tokens[3];
                    
                    // Traduzione per finestre e frigo
                    if (devices[i].type == DEVICE_WINDOW || devices[i].type == DEVICE_FRIDGE) {
                        out_label = (strcmp(tokens[3], "on") == 0) ? "open" : "close";
                        out_pos = "on"; 
                    }
                    
                    // Invio il comando formattato con l'identità del Controller
                    snprintf(cmd, sizeof(cmd), "%s %s %s %d %d", CMD_SWITCH, out_label, out_pos, CONTROLLER_ID, DEVICE_CONTROLLER);
                    ipc_send_message(fd_child, cmd);
                    close(fd_child);
                }
            }
        }
        return;
    }
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
    if (id == CONTROLLER_ID) {
        int num = count_direct_children(CONTROLLER_ID);
        printf(
            "\n------- Controller Details -----\n"
            "ID: %d\n"
            "State: %s\n"
            "Switches: main=%s\n"
            "Registry: num=%d\n"
            "----------------------------\n\n",
            CONTROLLER_ID,
            controller_state ? "On" : "Off",
            controller_state ? "on" : "off",
            num
        );
        return;
    }
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

            if (strncmp(controller_buf, CMD_INFO, strlen(CMD_INFO)) == 0) {
                pthread_mutex_lock(&mutex);
                printf("\n\r\033[KManual Override! Requesting Controller Info...");
                device_info(CONTROLLER_ID);
                printf("domotics> ");
                fflush(stdout);
                pthread_mutex_unlock(&mutex);
                continue;
            }
            else if (strncmp(controller_buf, CMD_SWITCH, strlen(CMD_SWITCH)) == 0) {
                char cmd[32], label[32], pos[32];
                if (sscanf(controller_buf, "%s %s %s", cmd, label, pos) >= 3) {
                    // Ricostruisce i token come se fossero stati scritti da tastiera
                    char *tokens[] = {"switch", "0", label, pos};
                    pthread_mutex_lock(&mutex);
                    printf("\n\r\033[KManual Override! Executing Switch on Controller...\n");
                    if (switch_check(tokens, 4)) {
                        switch_device(tokens);
                    }
                    printf("domotics> ");
                    fflush(stdout);
                    pthread_mutex_unlock(&mutex);
                }
                continue;
            }
            else if (strncmp(controller_buf, CMD_DELETE, strlen(CMD_DELETE)) == 0) {
                pthread_mutex_lock(&mutex);
                printf("\n\r\033[KManual Override! Error: Cannot delete the Controller.\n");
                printf("domotics> ");
                fflush(stdout);
                pthread_mutex_unlock(&mutex);
                continue;
            }
            
            //messagi dai dispositivi figli
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

    //registrazione controller come device
    devices[device_count].id = CONTROLLER_ID;
    devices[device_count].pid = getpid();
    devices[device_count].type = DEVICE_CONTROLLER;
    devices[device_count].parent_id = -1; // il Controller è la radice: non ha un parent
    devices[device_count].fifo_fd = -1;
    device_count++;

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

        //evita race condition
        sigset_t mask, oldmask;
        sigemptyset(&mask);
        sigaddset(&mask, SIGCHLD);
        sigprocmask(SIG_BLOCK, &mask, &oldmask);

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
            break;
        }
        else {
            printf("Invalid command.\n");
        }

        //ripristino maschera
        sigprocmask(SIG_SETMASK, &oldmask, NULL);
        
    } 
    
    printf("\nShutting down Domotics System gracefully...\n");

    // Per sbloccare il listener_thread dalla read
    int fd = open(FIFO_CONTROLLER, O_WRONLY | O_NONBLOCK);
    if (fd != -1) { 
        write(fd, " ", 1); 
        close(fd); 
    }

    // Aspettiamo che il thread finisca pulito
    pthread_join(listener_td, NULL);
    
    // Killa tutti i processi figli e cancella i file .fifo
    cleanup_all_devices();
}
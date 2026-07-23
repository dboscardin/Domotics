#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "controller.h"
#include "device.h"
#include "bulb.h"
#include "window.h"
#include "fridge.h"
#include "ipc.h"
#include "hub.h"
#include "protocol.h"

#define MAX_CMD_LEN 50
#define MAX_DEVICES 50
#define MAX_TOKENS 10

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
}

// Gestione della chiusura tramite Ctrl+C
static void handle_sigint(int sig) {
    (void)sig;
    printf("\nInterrupted. Terminating all background devices...\n");
    cleanup_all_devices();
    exit(0);
}

// Handler del segnale SIGCHLD quando un device viene eliminato al di fuori del programma
static void handle_sigchld(int sig) {
    (void)sig;
    int status;
    pid_t pid;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        // Cerco il dispositivo con questo PID e lo rimuovo dall'array
        for (int i = 0; i < device_count; i++) {
            if (devices[i].pid == pid) {
                if (devices[i].fifo_fd != -1) {
                    close(devices[i].fifo_fd);
                }
                ipc_remove_fifo(devices[i].id, devices[i].type);
                
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
    if(fgets(buffer, size, stdin) == NULL) {
        return 0;
    }

    //toglie /n finale
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
            default:
                _exit(1);
        }
        _exit(0);
    }

    devices[device_count].id = curr_id;
    devices[device_count].pid = pid;
    devices[device_count].type = type;
    devices[device_count].parent_id = -1;

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

static void link_devices(int child_id, int hub_id) {
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

    if (devices[hub_idx].type != DEVICE_HUB) {
        printf("Error: device ID %d is not a Hub.\n\n", hub_id);
        return;
    }

    printf("Link request sent: Device %d -> Hub %d\n", child_id, hub_id);
    fflush(stdout);

    //invia messaggio all'hub. tramite fifo
    char msg[64];
    snprintf(msg, sizeof(msg), "LINK_CHILD %d %d", child_id, devices[child_idx].type);
    int fd = ipc_open_for_writing(hub_id, DEVICE_HUB);
    if (fd != -1) {
        ipc_send_message(fd, msg);
        close(fd);
        devices[child_idx].parent_id = hub_id;

        usleep(50000); //50ms

    } else {
        printf("Error: failed to connect to Hub %d FIFO.\n\n", hub_id);
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

    if (devices[hub_idx].type != DEVICE_HUB) {
        printf("Error: device ID %d is not a Hub.\n\n", hub_id);
        return;
    }

    printf("Unlink request sent: Device %d from Hub %d\n", child_id, hub_id);
    fflush(stdout);

    char msg[64];
    snprintf(msg, sizeof(msg), "UNLINK_CHILD %d", child_id);
    int fd = ipc_open_for_writing(hub_id, DEVICE_HUB);
    if (fd != -1) {
        ipc_send_message(fd, msg);
        close(fd);
        devices[child_idx].parent_id = -1;
        usleep(50000); // 50ms
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

// Rimuove ricorsivamente i figli associati a un hub
static void remove_children_from_hub(int parent_id) {
    for (int i = 0; i < device_count; i++) {
        if (devices[i].parent_id == parent_id) {
            int child_id = devices[i].id;
            
            if (devices[i].type == DEVICE_HUB) {
                remove_children_from_hub(child_id);
            }
            
            remove_device_from_array(child_id);
            i--;
        }
    }
}

static void remove_device(int id) {
    int index = find_device_by_id(id);
    if (index == -1) {
        printf("No device with this Id.\n\n");
        return;
    }

    DeviceType type = devices[index].type;
    pid_t pid = devices[index].pid;

    // Invio DELETE tramite fifo
    char msg[] = "DELETE";
    int fd = ipc_open_for_writing(id, type);
    if (fd != -1) {
        ipc_send_message(fd, msg);
        close(fd);
    } else {
        kill(pid, SIGKILL);
    }
    
    //attesa bloccante
    int status;
    waitpid(pid, &status, 0);

    //Caso hub
    if (type == DEVICE_HUB) {
        remove_children_from_hub(id);
    }

    remove_device_from_array(id);
    printf("Device ID: %d is removed\n\n", id);
    fflush(stdout);

}
static bool switch_check(char *tokens[], int count) {
    //check array size
    if(count != 4) {
        return false;
    } 
    //check if it's  valid ID
    int id = parse_id(tokens[1]);
    if(id == -1) {
        return false;
    }
    //check if it works on a right attribute 
    char *registers[] = {"power", "time", "is_open", "delay", "perc", "temp", "thermostat"};
    bool labelFound = false;
    for(int i = 0; i < sizeof(registers) / sizeof(registers[0]); i++) {
        if(strcmp(tokens[2], registers[i]) == 0) {
            labelFound = true;
        }
    }
    if(!labelFound) { 
        return false;
    }
    if(strcmp(tokens[3], "on") == 0 || strcmp(tokens[3], "off") == 0 ) {
        return true;
    }
    char *endptr;
    //tries to convert in int
    strtol(tokens[3], &endptr, 10);

    //se endptr punta alla fine, tutta la stringa era un numero
    return (*endptr == '\0');
}

static void switch_device(char *tokens[]) {
    int id = parse_id(tokens[1]);
    int index = find_device_by_id(id);
    if(index = -1) return;

    char message[MAX_MSG_LEN];
    snprintf(message, sizeof(message), "SWITCH %s %s", tokens[2], tokens[3]);

    int fd = ipc_open_for_writing(id, devices[index].type);
    if(fd != -1) {
        ipc_send_message(fd, message);
        close(fd);
        printf("Command sent to Device %d: SWICTH %s % s.\n", id, tokens[2], tokens[3]);
    } else {
        printf("Error: Failed to communicate with Device %d.\n", id);
    }
}

static void device_info(int id) {
    int index = find_device_by_id(id);
    printf("%d --> Id=%d, Pid=%d, Type=%s\n", (index + 1), devices[index].id, devices[index].pid, device_type_to_string(devices[index].type));
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
void controller_run(void) {

    //Ctrl+C
    struct sigaction sa;
    sa.sa_handler = handle_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);


    //SIGCHLD
    struct sigaction sa_child;
    sa_child.sa_handler = handle_sigchld;
    sigemptyset(&sa_child.sa_mask);
    sa_child.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &sa_child, NULL);



    char buffer[MAX_CMD_LEN];  

    printf("What do you want to do?\n");
    while(true) {

        printf("domotics> ");

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
            cleanup_all_devices();
            return;
        }
        else {
            printf("Invalid command.\n");
        }
        
    }   
}
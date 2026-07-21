#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "controller.h"
#include "device.h"
#include "bulb.h"
#include "window.h"
#include "fridge.h"
#include "ipc.h"

#define MAX_CMD_LEN 50
#define MAX_DEVICES 50
#define MAX_TOKENS 10

static DeviceInfo devices[MAX_DEVICES];
static int device_count = 0;    //conta dispositivi attuali
static int curr_id = 0;         //assegna un id che non decrementa all'eliminazione
//devo averne due per evitare conflitti causa eliminazione

static void devices_list(void);
static void add_device(char* device);

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
            printf("%d --> Id=%d, Pid=%d, Type=%s\n", (i + 1), devices[i].id, devices[i].pid, device_type_to_string(devices[i].type));
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
            default:
                _exit(1);
        }
        _exit(0);
    }

    //il padre apre in scrittura per comandi futuri
    int wfd = ipc_open_for_writing(curr_id, type);

    devices[device_count].id = curr_id;
    devices[device_count].pid = pid;
    devices[device_count].type = type;
    devices[device_count].fifo_fd = wfd;

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

static void remove_device(int id) {

    int index = find_device_by_id(id);
    if(index == -1) {
        printf("No device with this Id.\n");
        return;
    }

    kill(devices[index].pid, SIGTERM);
    waitpid(devices[index].pid, NULL, 0);

    for (int i = index; i < device_count -1; i++)
    {
        devices[i] = devices[i + 1];
    }

    device_count--;

    printf("Device id=%d removed successfully.\n", id);
}

static void device_info(int id) {
    int index = find_device_by_id(id);
    printf("%d --> Id=%d, Pid=%d, Type=%s\n", (index + 1), devices[index].id, devices[index].pid, device_type_to_string(devices[index].type));
}

static void commands() {
    printf("Commands list:\n");
    printf("list: Lists all devices.\n");
    printf("add <device>: Spawns a new device in the house. (Max 50 devices)\n");
    printf("del <id>: Delete an existing device.\n");
    printf("link <id1> to <id2>: id1 will be controlled by id2.\n");
    printf("switch <id> <label> <pos>: Sets the switch label of device id to position pos.\n");
    printf("info <id>: Displays the complete details of the device\n");
    printf("quit: To quit the program.\n");
}
void controller_run() {
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
            if(count != 4) {
                printf("Invalid command. Structure should be: link <id1> to <id2>. \n");
            }
            printf("This feature will be avaliable soon!\n");
        }
        else if(strcmp(tokens[0], "switch") == 0) {
            bool isCommandOk = true;
            if(count != 4) {
                isCommandOk = false;
            } 
            int id = parse_id(tokens[1]);
            if(id == -1) {
                isCommandOk = false;
            }
            char *registers[] = {"power", "time", "is_open", "delay", "perc", "temp", "thermostat"};
            bool labelFound = false;
            for(int i = 0; i < sizeof(registers); i++) {
                if(strcomp(tokens[2], registers[i]))
                    labelFound = true;
            }
            if(!labelFound) {
                isCommandOk = false;
            }

            /*if(isCommandOk) {
                switchDevice(int id, char *label, bool pos);
            } else {
                printf("Invalid command. Structure should be: switch <id> <label> <pos>. \n");
            }*/
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
            return;
        }
        else {
            printf("Invalid command.\n");
        }
    }

}
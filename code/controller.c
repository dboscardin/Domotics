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

#define MAX_CMD_LEN 16
#define MAX_DEVICES 50

static DeviceInfo devices[MAX_DEVICES];
static int device_count = 0;    //conta dispositivi attuali
static int curr_id = 0;         //assegna un id che non decrementa all'eliminazione
//devo averne due per evitare conflitti causa eliminazione

static void devices_list(void);
static void add_device(DeviceType type);
static void add_device_menu();

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

static void controller_menu(void) {
    printf("What do you want to do?\n");
    printf("[1] Show devices list\n");
    printf("[2] Add a new device\n");
    printf("[3] Delete a device\n");
    printf("[4] Link two devices\n");
    printf("[5] Switch a device\n");
    printf("[6] Get more info\n");
    printf("[7] Quit\n");
}

static void devices_list(void) {
    if(device_count == 0)
        printf("No devices yet\n\n");
    else {
        for (int i = 0; i < device_count; i++)
        {
            printf("%d --> Id=%d, Pid=%d, Type=%s\n", (i + 1), devices[i].id, devices[i].pid, device_type_to_string(devices[i].type));
        }
        printf("\n");
    }
}

static void add_device(DeviceType type) {
    if(device_count >= MAX_DEVICES) {
        printf("You reached the limit of devices.\n\n");
        return;
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("Error during fork.");
        return;
    }
    
    if(pid==0) {
        switch (type)
        {
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

    devices[device_count].id = curr_id;
    devices[device_count].pid = pid;
    devices[device_count].type = type;


    /*switch (type)
    {
        case DEVICE_BULB:
            printf("Bulb ");
            break;

        case DEVICE_WINDOW:
            printf("Window ");
            break;

        case DEVICE_FRIDGE:
            printf("Fridge ");
            break;
    }*/    
    printf("%s", device_type_to_string(type));    
    printf(" created successfully!\nid=%d, pid=%d\n\n", curr_id, pid);

    device_count++;
    curr_id++;

    //printf("Do you want to create another device? [y/n\n]");
    //printf("> ");
    
    controller_menu();
}

static void add_device_menu(void) {
    char buffer[MAX_CMD_LEN];

    printf("What do you want to create?\n");
    printf("[1] A bulb\n");
    printf("[2] A window\n");
    printf("[3] A fridge\n");
    printf("[4] Back\n");
    printf("> ");

    if(!read_line(buffer, sizeof(buffer))) {
        printf("Exit...");
        return;
    }

    switch (buffer[0])
    {
    case '1':
        add_device(DEVICE_BULB);
        break;
    case '2':
        add_device(DEVICE_WINDOW);
        break;
    case '3':
        add_device(DEVICE_FRIDGE);
        break;
    case '4':
    case '\0':
        return;
    default:
        printf("Invalid choice.\n");
        return;
    }

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
        printf("No device with this Id.");
        return;
    }

    kill(devices[index].pid, SIGTERM);
    waitpid(devices[index].pid, NULL, 0);

    for (int i = index; i < device_count; i++)
    {
        devices[i] = devices[i + 1];
    }

    device_count--;

    printf("Device id=%d removed successfully.\n\n", id);
}

static void remove_device_menu() {
    char buffer[MAX_CMD_LEN];
    char *endptr;
    long id;

    printf("What device you want to remove?\n");
    printf("ID> ");

    if(!read_line(buffer, sizeof(buffer))) {
        printf("Exit...");
        return;
    }

    id = strtol(buffer, NULL, 10);

    if(endptr == buffer || *endptr != '\0') {
        printf("Invalid ID\n");
        return;
    }

    remove_device((int));
}

void controller_run() {
    char buffer[MAX_CMD_LEN];    

    controller_menu();
    while(true) {

        printf("domotics> ");

        if (!read_line(buffer, sizeof(buffer))) {
            printf("Exit...\n\n");
            break;
        }

        switch (buffer[0])
        {
            case '1':
                devices_list();
                break;
            case '2':
                add_device_menu();
                break;
            case '3':
                remove_device_menu();
                break;
            case '4':
            case '5':
            case '6':   
                printf("This feature will be avaliable soon!\n\n");
                break;
            case '7':
                printf("Exit...\n\n");
                return;
            case '\0':
                break;     
            default:
                printf("Invalid choice.\n\n");
            break;
        }
    }

}
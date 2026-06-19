#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "controller.h"
#include "device.h"
#include "bulb.h"

#define MAX_CMD_LEN 16
#define MAX_DEVICES 50

static DeviceInfo devices[MAX_DEVICES];
static int device_count = 0;
//static int curr_id = 0;

static void add_bulb(void);
static void list(void);

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

static void list(void) {
    printf("No devices yet\n");
}

static void add_bulb() {
    if(device_count >= MAX_DEVICES) {
        printf("You reached the limit of devices.\n");
        return;
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("Error during fork.\n");
        return;
    }
    else if(pid==0)
    {
        create_bulb(device_count);
        _exit(0);
        
    } else {
        devices[device_count].id = device_count;
        devices[device_count].pid = pid;
        devices[device_count].type = DEVICE_BULB;
    }

    printf("Bulb created successfully!\nid=%d, pid=%d", device_count, pid);

    device_count++;
}

void controller_run() {
    char buffer[MAX_CMD_LEN];

    printf("Controller avviato, PID=%d\n", getpid());
    controller_menu();

    while(true) {
        printf("domotics> ");

        if(fgets(buffer, sizeof(buffer), stdin) == NULL) {
            printf("Exit...");
            break;
        }

        //toglie /n finale
        buffer[strcspn(buffer, "\n")] = '\0';

        if(strcmp(buffer, "1") == 0) {
            list();
        }
        else if(strcmp(buffer, "2") == 0) {
            add_bulb();
            printf("This feature will be avaliable soon!\n");
        }
        else if(strcmp(buffer, "3") == 0) {
            printf("This feature will be avaliable soon!\n");
        }
        else if(strcmp(buffer, "4") == 0) {
            printf("This feature will be avaliable soon!\n");
        }
        else if(strcmp(buffer, "5") == 0) {
            printf("This feature will be avaliable soon!\n");
        }
        else if(strcmp(buffer, "6") == 0) {
            printf("This feature will be avaliable soon!\n");
        }
        else if(strcmp(buffer, "7") == 0) {
            printf("Exit...\n");
            break;
        } else if (buffer[0] == '\0') {
            // riga vuota, ignora
            continue;
        } else {
            printf("Invalid choice, please try again.\n");
        }

    }
}
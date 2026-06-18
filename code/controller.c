#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "controller.h"

#define MAX_CMD_LEN 16

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

void controller_run() {
    char buffer[MAX_CMD_LEN];

    controller_menu();

    while(true) {
        printf("> ");

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
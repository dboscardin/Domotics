#include <stdio.h>
#include <sys/types.h>  //pid_t
#include <sys/wait.h>   //_exit, fork
#include <unistd.h>     //exit
#include <errno.h>   /* errno */
#include "controller.h"

int main(void) {
    //printf("Controller avviato\n");
    controller_run();

    return 0;
}
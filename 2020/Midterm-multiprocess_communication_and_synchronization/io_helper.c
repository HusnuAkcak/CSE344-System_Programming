#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
//------------------
#include "io_helper.h"

void
print_usage(char *process_name){

    char *template = "Usage : %s -N 3 -T 5 -S 4 -L 13 -U 10 -G 2 -F filePath\n";
    char message[strlen(template)+PROCESS_NAME_LEN];

    snprintf(message, strlen(template)+PROCESS_NAME_LEN, template, process_name);

    write(STDERR_FILENO, message, strlen(message));
}

int
open_file(char * file_path){
    mode_t mode = S_IRUSR | S_IRGRP | S_IROTH;

    int fd = open(file_path, O_RDONLY, mode);

    if(fd == -1)
        err_exit("File could not be opened.\n");

    return fd;
}

void
err_exit(char* error_msg){

    write(STDERR_FILENO, error_msg, strlen(error_msg));
    exit(1);
}

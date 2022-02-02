#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <semaphore.h>
#include "io_helper.h"
#include "base.h"
#include "supplier.h"

void
supply(char* filename, shMem* mem){

    int fd; // for truck file
    int cP=0, cC=0, cD=0; // counters for types of plates to use pre-control
    char buff[1]; // each time hold one char

    char* enterTemp = "The supplier is going to the kitchen to deliver %s --- kitchen items P:%d,C:%d,D:%d = %d\n";
    char* afterTemp = "The supplier delivered %s – after delivery ----------- kitchen items P:%d,C:%d,D:%d = %d\n";
    char* goodByMsg = "The supplier finished supplying – GOODBYE!\n";

    int msgLen = strlen(enterTemp)+60;
    char msg[msgLen];

    // control the truck, controlling count of plates
    fd = open_file(filename);
    while(read(fd, &buff, 1)==1){
        switch(buff[0]){
            case 'P':
                ++cP;
                break;
            case 'C':
                ++cC;
                break;
            case 'D':
                ++cD;
                break;
        }
    }
    close(fd);

    if(!(cP==cC && cC==cD && cP==M*L)) {

        char* errMsgTemp = "In the truck : P:%d, C:%d, D:%d\n"
                "M is %d.\n"
                "P, C, D must be equal to M and each other.\n";
        char errMsg[strlen(errMsgTemp)+41];

        snprintf(errMsg, strlen(errMsgTemp)+41, errMsgTemp, cP, cC, cD, M);


        sem_wait(&(mem->mSupply));
        mem->supply = false; // we notify the other processes that there will be no supplying
        sem_post(&(mem->mSupply));
        err_exit(errMsg);
    }

    //if number of each type of plate in the truck is equal to each other
    //and number of students(M) process starts to work
    fd = open_file(filename);
    while(read(fd, buff, 1) == 1){
        // wait for a place from the kitchen
        // fprintf(stderr, "]]]] before supplier wait for kTotalPlate\n");
        sem_wait(&(mem->kTotalPlate));
        // fprintf(stderr, "]]]] after supplier wait for kTotalPlate\n");

        switch (buff[0]) {
            case 'P':
                sem_post(&(mem->kSoup));

                sem_wait(&(mem->mPlate));
                snprintf(msg, msgLen, enterTemp, "soup", mem->p, mem->c, mem->d, ((mem->p) +(mem->c) +(mem->d)));
                write(STDERR_FILENO, msg, strlen(msg));
                ++(mem->p);
                snprintf(msg, msgLen, afterTemp, "soup", mem->p, mem->c, mem->d, ((mem->p) +(mem->c) +(mem->d)));
                write(STDERR_FILENO, msg, strlen(msg));
                sem_post(&(mem->mPlate));
                break;
            case 'C':
                sem_post(&(mem->kMainCourse));

                sem_wait(&(mem->mPlate));
                snprintf(msg, msgLen, enterTemp, "main course", mem->p, mem->c, mem->d, ((mem->p) +(mem->c) +(mem->d)));
                write(STDERR_FILENO, msg, strlen(msg));
                ++(mem->c);
                snprintf(msg, msgLen, afterTemp, "main course", mem->p, mem->c, mem->d, ((mem->p) +(mem->c) +(mem->d)));
                write(STDERR_FILENO, msg, strlen(msg));
                sem_post(&(mem->mPlate));
                break;
            case 'D':
                sem_post(&(mem->kDessert));

                sem_wait(&(mem->mPlate));
                snprintf(msg, msgLen, enterTemp, "dessert", mem->p, mem->c, mem->d, ((mem->p) +(mem->c) +(mem->d)));
                write(STDERR_FILENO, msg, strlen(msg));
                ++(mem->d);
                snprintf(msg, msgLen, afterTemp, "dessert", mem->p, mem->c, mem->d, ((mem->p) +(mem->c) +(mem->d)));
                write(STDERR_FILENO, msg, strlen(msg));
                sem_post(&(mem->mPlate));
                break;
        }

    }

    close(fd);
    sem_wait(&(mem->mSupply));
    mem->supply = false;
    sem_post(&(mem->mSupply));
    write(STDERR_FILENO, goodByMsg, strlen(goodByMsg));

}

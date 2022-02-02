#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <signal.h>
#include <errno.h>
#include <wait.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/stat.h>       /* For mode constants */
#include <fcntl.h>          /* For O_* constants */
#include <semaphore.h>
#include <time.h>

// ENUMS, STRUCTS AND MACROS START /////////////////////////////////////////////
#define DEBUG 0
#define NAME_MAX_LEN 255
#define LINE_MAX_LEN 1024
#define MAX_NUM_OF_PROCESS 200
#define NAMED_SEM_INITAL_VALUE 1
#define FIFO_PERM (S_IRUSR | S_IWUSR)


struct fifoInfo{
    char name[NAME_MAX_LEN];
    int ownerPid;
    int readEndLastOpened; // if the value is 1, first open WR end of the other fifos then open RD end of this fifo
};

struct potatoInfo{
    int id; 
    int temperature; // current temperature
    int doneSwitchNumber;
};

struct message{
    int senderPid;
    int potatoId;
};

struct sharedMem{
    sem_t semFifoBarrier;
    int numOfProceses;
    struct fifoInfo fifos[MAX_NUM_OF_PROCESS];
    struct potatoInfo potatos[MAX_NUM_OF_PROCESS];
};
// ENUMS, STRUCTS AND MACROS END ///////////////////////////////////////////////

// FUNCTION PROTOTYPES START ////////////////////////////////
int waitForFifoOpen(struct sharedMem* shm, struct fifoInfo* fifoInfoArr, int numOfProceses, int *fifoFds, char **fifoNames);
void closeFds(int *fds, int size);
void unlinkFifos(struct fifoInfo* fifoInfoArr, int size);
int coolDownPotatos(struct sharedMem *shm, sem_t *sem, int initWithPotato, int *fifoFds, char **fifoNames, int numOfFifos);
void deleteNewLines(char *str);
// FUNCTION PROTOTYPES END  /////////////////////////////////

// GLOBAL VARIABLES START ///////////////////////////////////
sig_atomic_t exitGracefully = 0;
sig_atomic_t sigInt = 0;
// GLOBAL VARIABLES END /////////////////////////////////////

void handler(int sigNo){
    ++sigInt;
    // fprintf(stderr, "SIGCHLD handled.\n");
}

int main(int argc, char * argv[]){

    if(argc != 9){
        fprintf(stderr, "USAGE: %s –b haspotatoornot –s nameofsharedmemory –f filewithfifonames –m namedsemaphore\n", argv[0]);
        return -1;
    }
    srand(time(NULL));  

    // unuque suffix for named semaphore and shared memory
    char uniqueSuffix[NAME_MAX_LEN] = "161044112";

    struct sharedMem *shm = NULL;
    char *nameOfSharedMem = NULL;
    long shmMemSize = 0;

    char *semName = NULL;
    sem_t *sem = NULL;

    int numOfProcess = 0,
        hotPotatos,
        fdOfFifoFile = -1;

    struct fifoInfo* localFifoArr = NULL;
    int *fifoFds; // 0 th index READ end, rest of it WRITE ends, size = numOfProceseses
    char **fifoNames;
    
    char optB[NAME_MAX_LEN];
    char optS[NAME_MAX_LEN];
    char optM[NAME_MAX_LEN];
    int option;
    while ((option = getopt(argc, argv, "b:s:f:m:")) != -1) {
        switch (option) {
            case 'b': 
                strcpy(optB, optarg);
                hotPotatos = atoi(optB); 
                if(hotPotatos == 0 && strcmp("0", optB) != 0){
                    fprintf(stderr, "Hot potatos could not be read\n");
                    exitGracefully = 1;
                }
                break;
            case 's':
                strcpy(optS, optarg);
                nameOfSharedMem = strcat(optS, uniqueSuffix);
                break;
            case 'm':
                strcpy(optM, optarg);
                if(strlen(optM) != 0){
                    semName = strcat(optM, uniqueSuffix);
                    if((sem = sem_open(semName, O_RDWR | O_CREAT, 0666, NAMED_SEM_INITAL_VALUE))== SEM_FAILED && errno != EEXIST){                        
                        perror("sem_open error");
                        exitGracefully = 1;
                    }
                }
                break;
            case 'f': 
                fdOfFifoFile = open(optarg, O_RDONLY);
                if(fdOfFifoFile == -1){
                    perror("File open error");
                    exitGracefully = 1;
                }
            
                break;
            default: 
                fprintf(stderr, "USAGE: %s –b haspotatoornot –s nameofsharedmemory –f filewithfifonames –m namedsemaphore\n", argv[0]);
                return -1;        
        }
    }

    if(DEBUG){
        fprintf(stderr, "number of potatos: %d\n", hotPotatos);
        fprintf(stderr, "fifonamesfile fd: %d\n", fdOfFifoFile);
        fprintf(stderr, "name of shared memory %s\n", nameOfSharedMem);
    }

    if(exitGracefully == 1){
        // clean up 
        sem_close(sem);
        sem_unlink(semName);
        close(fdOfFifoFile);
        fprintf(stderr, "exitGracefully is 1, so clean up and terminate\n");
        return -1;
    }

    struct sigaction sact;
    if(sigemptyset(&sact.sa_mask) != 0){
        perror("sigemptyset() error");
        return 1;
    }
    sact.sa_flags = 0;
    sact.sa_handler = handler;
    if(sigaction(SIGINT, &sact, NULL) != 0){
        perror("sigaction() error");
        sem_close(sem);    
        sem_unlink(semName);
        close(fdOfFifoFile);
        return -1;
    }

    flock(fdOfFifoFile, LOCK_EX);

    char line[LINE_MAX_LEN];
    FILE* fp = fdopen(fdOfFifoFile, "r");
    if(fp == NULL){
        perror("fdopen error");
        sem_close(sem);
        sem_unlink(semName);
        close(fdOfFifoFile);
        return -1;
    }

    // to know number of pipes and therefore number of processes, lines are counting
    numOfProcess =0;
    while(fgets(line, LINE_MAX_LEN+1, fp) != NULL){ ++numOfProcess; }

    if(numOfProcess > MAX_NUM_OF_PROCESS){
        fprintf(stderr, "Maximum supported number of processes are %d\n", MAX_NUM_OF_PROCESS);
        sem_close(sem);
        sem_unlink(semName);
        fclose(fp);
        close(fdOfFifoFile);
        free(localFifoArr);
        return 0;
    }

    rewind(fp);// this fp is used one more time
    shmMemSize = sizeof(struct sharedMem);
    localFifoArr = (struct fifoInfo*)calloc(numOfProcess, sizeof(struct fifoInfo));

    if(localFifoArr == NULL){ 
        perror("calloc error");
        sem_close(sem);
        sem_unlink(semName);
        fclose(fp);
        close(fdOfFifoFile);
        free(localFifoArr);
        return -1;
    }

    // if(sigInt != 0){
    //     fprintf(stderr, "SIGINT is arrived\n");
    //     sem_close(sem);
    //     sem_unlink(semName);
    //     fclose(fp);
    //     close(fdOfFifoFile);
    //     free(localFifoArr);
    //     return -1;
    // }

    // create shared memory 
    int shmFd;
    if((shmFd =shm_open(nameOfSharedMem, O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR))== -1 && errno != EEXIST){
        perror("shm_open error");
        fclose(fp);
        sem_close(sem);
        sem_unlink(semName);
        close(fdOfFifoFile);
        free(localFifoArr);
        return -1;
    }
    else if(errno == EEXIST){
        if (DEBUG) fprintf(stderr, "shm is created already\n");

        if((shmFd =shm_open(nameOfSharedMem, O_RDWR, S_IRUSR | S_IWUSR)) == -1){
            perror("latter shm_open error");
            fclose(fp);
            sem_close(sem);
            sem_unlink(semName);
            close(fdOfFifoFile);
            free(localFifoArr);
            return -1;
        }

        shm = mmap(NULL, shmMemSize, PROT_READ | PROT_WRITE, MAP_SHARED, shmFd, 0);

        sem_wait(sem);
        // choose a fifo to read 
        int fi =0;
        if(DEBUG){
            struct stat s;
            if(fstat(shmFd, &s) != 0){
                perror("fstat error");
            }
            fprintf(stderr, "shm size: %ld\n", s.st_size);
            fprintf(stderr, "numOfProcess %d\n", numOfProcess);
            fprintf(stderr, "shm->fifos[0].opid %d\n", shm->numOfProceses);
            fprintf(stderr, "shmMemSize %ld\n", shmMemSize);
        }

        while(fi<numOfProcess && shm->fifos[fi].ownerPid != 0){ ++fi; }
        shm->fifos[fi].ownerPid = getpid();
        memcpy(localFifoArr, shm->fifos, numOfProcess*sizeof(struct fifoInfo));
        if(mkfifo(shm->fifos[fi].name, FIFO_PERM) == -1){
            perror("mkfifo error");
            fclose(fp);
            free(localFifoArr);
            sem_close(sem);
            sem_unlink(semName);
            close(fdOfFifoFile);
            munmap(nameOfSharedMem, shmMemSize);
            shm_unlink(nameOfSharedMem);
            return -1;
        }

        if(hotPotatos > 0){
            // potato 
            int pi =0;
            while(pi<numOfProcess && shm->potatos[pi].id != 0){ ++pi; }
            shm->potatos[pi].id = getpid();
            shm->potatos[pi].temperature = hotPotatos-1;
            shm->potatos[pi].doneSwitchNumber =1;
        }
        sem_post(&(shm->semFifoBarrier));
        sem_post(sem);
    }
    else{ // first created process 
        if(DEBUG){
            fprintf(stderr, "shm is created just now\n");
            fprintf(stderr, "ftruncate size %ld\n", shmMemSize);
        }

        sem_wait(sem);
        struct sharedMem shmLocal;
        for(int i=0; i<numOfProcess; ++i){ 
            shmLocal.potatos[i].doneSwitchNumber = 0;
            shmLocal.potatos[i].id = 0;
            shmLocal.potatos[i].temperature = 0;
        }
        shmLocal.numOfProceses = numOfProcess;
        // shmLocal.potatos[0]..id = 22;
        
        if(ftruncate(shmFd, shmMemSize) == -1){
            perror("ftruncate error");
            fclose(fp);
            free(localFifoArr);
            sem_close(sem);
            sem_unlink(semName);
            close(fdOfFifoFile);
            close(shmFd);
            shm_unlink(nameOfSharedMem);
            return -1;
        }

        if(DEBUG){
            struct stat s;
            if(fstat(shmFd, &s) != 0){
                perror("fstat error");
            }
            fprintf(stderr, "shm size: %ld\n", s.st_size);
        }

     
        shm = (struct sharedMem*)mmap(NULL, shmMemSize, PROT_READ | PROT_WRITE, MAP_SHARED, shmFd, 0);
        if(shm == MAP_FAILED){
            perror("mmap errorr");
            fclose(fp);
            free(localFifoArr);
            sem_close(sem);
            sem_unlink(semName);
            close(fdOfFifoFile);
            close(shmFd);
            shm_unlink(nameOfSharedMem);
        }
        close(shmFd);
        memcpy(shm, &shmLocal, shmMemSize);
        
        if(DEBUG){
            shm->fifos[0].ownerPid = 33;
            shm->potatos[0].id = 223;
            fprintf(stderr, "[DEBUG] shm->potatos[0].id: %d ||| shm->fifos[0].ownerPid: %d\n", shm->potatos[0].id, shm->fifos[0].ownerPid);
            shm->fifos[0].ownerPid = 0;
            shm->potatos[0].id = 0;
        }

        char *currTok, *prevTok;
        int fifoIndex= 0;
        while(fgets(line, LINE_MAX_LEN+1, fp) != NULL){
            currTok = strtok(line, "/");
            while(currTok != NULL){
                prevTok = currTok;
                currTok = strtok(NULL, "/");
            }
            strcpy( shm->fifos[fifoIndex].name, prevTok);
            deleteNewLines(shm->fifos[fifoIndex].name);
            ++fifoIndex;
        }
        shm->fifos[0].ownerPid = getpid(); // get first fifo as reading end
        shm->fifos[0].readEndLastOpened = 1; // this process opens its READ end after opening all other pipes WRITE ends
        memcpy(localFifoArr, shm->fifos, numOfProcess*sizeof(struct fifoInfo));

        // create fifo file
        sem_init(&(shm->semFifoBarrier), 1, 0); 
        if(mkfifo(shm->fifos[0].name, FIFO_PERM) == -1){
            perror("mkfifo error");
            fclose(fp);
            free(localFifoArr);
            sem_close(sem);
            sem_unlink(semName);
            close(fdOfFifoFile);
            munmap(nameOfSharedMem, shmMemSize);
            shm_unlink(nameOfSharedMem);
            return -1;
        }

        if(hotPotatos > 0){
            shm->potatos[0].id = getpid();
            shm->potatos[0].temperature = hotPotatos-1; // since when it is given to another process, it will be decremented by 1
            shm->potatos[0].doneSwitchNumber = 1;
        }
        if(DEBUG){
            for(int i =0; i<numOfProcess; ++i){
                fprintf(stderr, "[DEBUG] shm->fifos[%d].name: %s \t\t      ownerPid: %d, readLastOpened: %d\n", i, shm->fifos[i].name, shm->fifos[i].ownerPid, shm->fifos[i].readEndLastOpened);
            }
        }
        sem_post(sem);
    }

    // if(sigInt != 0){
    //     fprintf(stderr, "SIGINT is arrived\n");
    //     fclose(fp);
    //     munmap(shm, numOfProcess*(sizeof(struct potatoInfo)+sizeof(struct fifoInfo)));
    //     shm_unlink(nameOfSharedMem);
    //     sem_close(sem);
    //     sem_unlink(semName);
    //     close(fdOfFifoFile);
    //     free(localFifoArr);
    //     return -1;
    // }

    flock(fdOfFifoFile, LOCK_UN);

    fifoFds = (int*)calloc(numOfProcess, sizeof(int));
    fifoNames = (char**)calloc(numOfProcess, sizeof(char*));
    if(fifoFds == NULL){
        perror("Calloc error");
        fclose(fp);
        munmap(shm, numOfProcess*(sizeof(struct potatoInfo)+sizeof(struct fifoInfo)));
        shm_unlink(nameOfSharedMem);
        sem_close(sem);
        sem_unlink(semName);
        close(fdOfFifoFile);
        free(localFifoArr);
        return -1;
    }

    if(waitForFifoOpen(shm, localFifoArr, numOfProcess, fifoFds, fifoNames) == -1){
        perror("Fifo error");
        fclose(fp);
        closeFds(fifoFds, numOfProcess);
        free(fifoFds);
        free(localFifoArr);
        free(fifoNames);
        munmap(shm, numOfProcess*(sizeof(struct potatoInfo)+sizeof(struct fifoInfo)));
        shm_unlink(nameOfSharedMem);
        sem_close(sem);
        sem_unlink(semName);
        close(fdOfFifoFile);
        unlinkFifos(localFifoArr, numOfProcess);
        return -1;
    }

    if(DEBUG){
        fprintf(stderr, "[Pid:%d] open fds\n", getpid());
        for(int i=0; i<numOfProcess; ++i){
            fprintf(stderr, "[%d]: %d\n", i, fifoFds[i]);
        }
        sem_wait(sem);
        int i=0;
        while(i<numOfProcess && shm->potatos[i].id > 0){
            fprintf(stderr, "potato [%d] is %d degree\n", shm->potatos[i].id, shm->potatos[i].temperature);
            i++;
        }
        sem_post(sem);
    }

    coolDownPotatos(shm, sem, hotPotatos>0, fifoFds, fifoNames, numOfProcess);

    // free all resources 
    fclose(fp);
    closeFds(fifoFds, numOfProcess);
    free(fifoFds);
    unlinkFifos(localFifoArr, numOfProcess);
    free(localFifoArr);
    free(fifoNames);
    munmap(shm, numOfProcess*(sizeof(struct potatoInfo)+sizeof(struct fifoInfo)));
    shm_unlink(nameOfSharedMem);
    sem_close(sem);
    sem_unlink(semName);
    close(fdOfFifoFile);
    return 0;
}

void 
closeFds(int *fds, int size){
    
    for(int i=0; i<size; ++i){
        close(fds[i]);
    }
}

void 
unlinkFifos(struct fifoInfo* fifoInfoArr, int size){

    for(int i=0; i<size; ++i){
        unlink(fifoInfoArr[i].name);
    }
}

int 
waitForFifoOpen(struct sharedMem* shm, struct fifoInfo* fifoInfoArr, int numOfProceses, int *fifoFds, char **fifoNames){
// read end of fd is at index 0

    int i=0, fd, fdIndex=1;
    while(i < numOfProceses && fifoInfoArr[i].ownerPid != getpid()){ ++i; }

    // first, open all other write ends
    if(fifoInfoArr[i].readEndLastOpened){

        // wait for fifo file creations
        for(int i=0; i<numOfProceses-1; ++i){ 
            sem_wait(&(shm->semFifoBarrier)); 
        }
        sem_destroy(&(shm->semFifoBarrier)); // no longer needed.

        // if(sigInt != 0){
        //     fprintf(stderr, "SIGINT is arrived\n");
        //     return -1;
        // }

        for(int fi=0; fi<numOfProceses; ++fi){
            if(fi != i){
                if(DEBUG) { fprintf(stderr, "pid %d opening [%s] WR end\n", getpid(), fifoInfoArr[fi].name); }

                fd = open(fifoInfoArr[fi].name, O_WRONLY);
                if(fd == -1){
                    return -1;
                }
                // if(sigInt != 0){
                //     fprintf(stderr, "SIGINT is arrived\n");
                //     return -1;
                // }
                fifoNames[fdIndex] = fifoInfoArr[fi].name;
                fifoFds[fdIndex++] = fd;
            }
        }

        if(DEBUG) { fprintf(stderr, "pid %d opening [%s] RD end\n", getpid(), fifoInfoArr[i].name); }
        fd = open(fifoInfoArr[i].name, O_RDONLY);
        if(fd == -1) { 
            return -1; 
        }
        fifoFds[0] = fd; // read end at zero index
        fifoNames[0] = fifoInfoArr[i].name;
    }
    else{ // first open read end of, then open others write ends

        if(DEBUG) { fprintf(stderr, "pid %d opening [%s] RD end\n", getpid(), fifoInfoArr[i].name); }

        fd = open(fifoInfoArr[i].name, O_RDONLY);
        if(fd == -1) { 
            return -1; 
        }
        fifoFds[0] = fd; // read end at zero index
        fifoNames[0] = fifoInfoArr[i].name;

        for(int fi=0; fi<numOfProceses; ++fi){
            if(fi != i){
                if(DEBUG) { fprintf(stderr, "pid %d opening [%s] WR end\n", getpid(), fifoInfoArr[fi].name); }

                fd = open(fifoInfoArr[fi].name, O_WRONLY);
                // if(sigInt != 0){
                //     fprintf(stderr, "SIGINT is arrived\n");
                //     return -1;
                // }
                if(fd == -1){
                    return -1;
                }
                fifoNames[fdIndex] = fifoInfoArr[fi].name;
                fifoFds[fdIndex++] = fd;
            }
        }
    }

    // if(sigInt != 0){
    //     fprintf(stderr, "SIGINT is arrived\n");
    //     return -1;
    // }

    return 0;
}

int 
coolDownPotatos(struct sharedMem *shm, sem_t *sem, int initWithPotato, int *fifoFds, char **fifoNames, int numOfFifos){

    int totalPotatos=0;
    // control if there is any potato in shared memory region
    sem_wait(sem); 

    if(sigInt != 0){
        fprintf(stderr, "SIGINT is arrived\n");
        return -1;
    }

    if(!initWithPotato){
        int i=0;
        while(i<numOfFifos){
            if(shm->potatos[i].id > 0){ // there is a potato, hot or cool
                ++totalPotatos;
            }
            ++i;
        }
    }
    sem_post(sem);

    if(!initWithPotato && totalPotatos == 0){
        return 0;
    }

    struct message msg;
    int cont = 1,
        potatoIndex, 
        fifoIndex;
    
    if(initWithPotato){
        int rand = random()%(numOfFifos-1);
        msg.potatoId = getpid();
        msg.senderPid = getpid();
        fprintf(stderr, "pid=%d sending potato number %d to %s; this is switch number %d\n",
                        getpid(), getpid(), fifoNames[rand+1], 1);
        fflush(stderr);
        write(fifoFds[rand+1], &msg, sizeof(struct message));
    }

    while(cont){
        potatoIndex = 0;
        fifoIndex = 0;
        if(read(fifoFds[0], &msg, sizeof(struct message)) == -1){
            perror("Fifo read error");
            return -1;
        }
        if(sigInt != 0){
            fprintf(stderr, "SIGINT is arrived\n");
            return -1;
        }
        if(DEBUG) { fprintf(stderr, "Message read %d, %d\n", msg.potatoId, msg.senderPid); }
       
        if(msg.potatoId == 0){
            if(DEBUG) { fprintf(stderr, "Termination message is received\n"); }
            cont = 0;
            continue;
        }
        
        sem_wait(sem);

        if(sigInt != 0){
            fprintf(stderr, "SIGINT is arrived\n");
            return -1;
        }
        
        while(fifoIndex < numOfFifos && (shm->fifos[fifoIndex].ownerPid) != getpid()){ ++fifoIndex; }
        while(potatoIndex < numOfFifos && (shm->potatos[potatoIndex].id) != msg.potatoId){ ++potatoIndex; }

        fprintf(stderr, "pid=%d receiving potato number %d from %s\n", getpid(), msg.potatoId, fifoNames[0]);
        fflush(stderr);

        int temp = shm->potatos[potatoIndex].temperature;
        if(temp>0){

            --(shm->potatos[potatoIndex].temperature);
            ++(shm->potatos[potatoIndex].doneSwitchNumber);
            
            msg.senderPid = getpid();
            int rand = random()%(numOfFifos-1);
            fprintf(stderr, "pid=%d sending potato number %d to %s; this is switch number %d\n",
                            getpid(), msg.potatoId, fifoNames[rand+1], (shm->potatos[potatoIndex].doneSwitchNumber));
            fflush(stderr);
            write(fifoFds[rand+1], &msg, sizeof(struct message));
        }
        else {

            fprintf(stderr, "pid=%d; potato number %d has cooled down.\n", getpid(), msg.potatoId);       
            fflush(stderr);
            //search for a hot potato 
            int i=0;
            while(i<numOfFifos && (shm->potatos[i].temperature == 0)){ ++i; }

            if(i == numOfFifos){// run out of hot potatoes
                for(int j=1; j<numOfFifos; ++j){
                    msg.senderPid = getpid();
                    msg.potatoId = 0; // termination sign
                    write(fifoFds[j], &msg, sizeof(struct message));
                }
                break;
            }
        }
        sem_post(sem);
    }
    
    return 0;
}

void 
deleteNewLines(char *str){

    for(int i=0; i<strlen(str); ++i){
        if(str[i] == '\n'){
            str[i] = 0;
        }
    }
}


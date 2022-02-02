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

// ENUMS, STRUCTS AND MACROS START /////////////////////////////////////////////
#define NAME_MAX_LEN 255
#define CITIZEN_MSG_LEN 300
#define SHM_STATIC_NAME   "/shmStatic161044112"
#define SHM_BUFF_NAME     "/shmBuff161044112"
#define SHM_CITIZENS_NAME "/shmCitizens161044112"
#define SHM_CHILD_IDS "/shmChildIds161044112"

struct ShmStatic {      
    int buffSize,           // current number of vaccine                
        buffCapacity,       // vaccine buffer capacity
        totalNumOfCitizens, 
        leftCitizens,       // remaining citizen
        terminate, // a sign for others to terminate
        leftNurses,// to know which nurse prints their termination message
        vaccinationSession, // t*c
        sigIntArrived;      // to tell the other proc that SIGINT is received

    sem_t semEmpty, // initial value is buffer capacity
        semCitizenAvailable, // num of citizens wating to be invited
        shmLock,    // lock all shared memory segment
        semVacc1,   // to notify and wait Vaccine '1'
        semVacc2,   // to notify and wait Vaccine '2'
        semVaccAvailable;  // at least a pair of '1' and '2' is available
};

struct Citizen{
    int pid;
    char gone,      // citizen leaved 
         inClinic;  // currently busy in clinic
};
// ENUMS, STRUCTS AND MACROS END ///////////////////////////////////////////////

// FUNCTION PROTOTYPES START ////////////////////////////////
int constituteSharedMem(int bufferCapacity, int numOfNurses, int numOfCitizen, int t);
int citizen(int citizenNo, int t, sigset_t sigsetSusUsr1);
int nurse(int nurseNo, int fd);
int vaccinator(int vaccinatorNo);
int pusherV1(int numOfV1);
int pusherV2(int numOfV2);
int removeChar(int capacity, char c);
int findFirstEmptyIndex();
int adjustHandler(int sigNo, void handler(int));
int getOldestAvailableCitizenPid();
int getCitizenIndex(int pid);
void destroyAllSemaphores();
void munmapSharedMems();
void unlinkSharedMems();
void waitChildren();
int countOf(char c);
int isExist(char c);
// FUNCTION PROTOTYPES END  /////////////////////////////////

// GLOBAL VARIABLES START ///////////////////////////////////
sig_atomic_t sigInt=0;
sig_atomic_t sigUsr1=0;
int numOfCitizens, capacityOfBuffer, numOfChildren;
struct ShmStatic* shmStatic; // conditional vars and unnamed semaphores
char *shmBuff;  // vaccine buffer
struct Citizen* shmCitizens; // to invite citizens to clinic
int *shmChildPids; // to be able to send signal between any processes
// GLOBAL VARIABLES END /////////////////////////////////////

// SIGNAL HANDLERS START ////////////////////////////////////
void handlerUsr1(int sigNo){
    ++sigUsr1;
}

void handlerIntChild(int sigNo){
    for(sig_atomic_t i=0; i<numOfChildren; ++i){
        kill(shmChildPids[i], SIGINT);
    }
    munmapSharedMems();
    exit(EXIT_SUCCESS);
}
void handlerIntParent(int sigNo){
    for(sig_atomic_t i=0; i<numOfChildren; ++i){
        kill(shmChildPids[i], SIGINT);
    }
    destroyAllSemaphores();
    munmapSharedMems();
    unlinkSharedMems();
    kill(0, getpid());
    // waitChildren();
    exit(EXIT_SUCCESS);
}
// SIGNAL HANDLERS ENDS  ////////////////////////////////////

int 
main(int argc, char*argv[]){

    if(argc != 13){
        fprintf(stderr, "USAGE: ./program –n 3 –v 2 –c 3 –b 11 –t 3 –i inputfilepath\n");
        return -1;
    }

    int numOfPushers= 2,
        numOfNurse,
        numOfVaccinator,
        t; // how many times each citizen will received the 2 shots

    char *filepath;

    int option;
    while((option = getopt(argc, argv, "n:v:c:b:t:i:"))!= -1){
        switch(option){
            case 'n':
                numOfNurse = atoi(optarg);
                break;
            case 'v':
                numOfVaccinator = atoi(optarg);
                break;
            case 'c':
                numOfCitizens = atoi(optarg);
                break;
            case 'b':
                capacityOfBuffer = atoi(optarg);
                break;
            case 't':
                t = atoi(optarg);
                break;
            case 'i':
                filepath = optarg;
                break;
        }
    }

    int inputFd = open(filepath, O_RDWR);
    if(inputFd == -1){
        perror("open error");
        return -1;
    }

    // for citizens to be invited
    if(adjustHandler(SIGUSR1, handlerUsr1) == -1){
        perror("adjustHandler error");
        close(inputFd);
        return -1;
    }

    if(adjustHandler(SIGINT, handlerIntParent) == -1){
        perror("adjustHandler error");
        close(inputFd);
        return -1;
    }
    

    sigset_t sigset;
    sigemptyset(&sigset);
    sigaddset(&sigset, SIGUSR1);
    sigprocmask(SIG_SETMASK, &sigset, NULL);

    sigset_t sigsetSusUsr1;
    if(sigfillset(&sigsetSusUsr1) != 0 || sigdelset(&sigsetSusUsr1, SIGUSR1) != 0){
        perror("sigfillset or sigdelset error");
        close(inputFd);
        return -1;
    }

    numOfChildren = numOfNurse + numOfVaccinator + numOfCitizens + numOfPushers-1;
    if(constituteSharedMem(capacityOfBuffer, numOfNurse, numOfCitizens, t) == -1){
        fprintf(stderr, "shared memory could not be constructed\n");
        close(inputFd);
        return -1;
    }

    fprintf(stderr, "Welcome to the GTU344 clinic. Number of citizens to vaccinate c=%d with t=%d doses.\n",
                    numOfCitizens,
                    t
    );    
    fflush(stderr);


    int index = 0;
    int i, childPid;
    for(i=0; i<numOfCitizens; ++i){
        switch (childPid = fork())
        {
            case 0:
                // setpgid(0, getppid()); // put all children in a group to send collective signal
                adjustHandler(SIGINT, handlerIntChild);
                citizen(i+1, t, sigsetSusUsr1);
                munmapSharedMems();
                _exit(EXIT_SUCCESS);
                break;
            default:
                sem_wait(&(shmStatic->shmLock));
                shmCitizens[i].pid = childPid;
                shmChildPids[index++] = childPid;
                sem_post(&(shmStatic->shmLock));
                break;
        }
    }

    for(i=0; i<numOfNurse; ++i){
        switch (childPid=fork())
        {
            case 0:
                // setpgid(0, getppid()); // put all children in a group to send collective signal
                adjustHandler(SIGINT, handlerIntChild);
                nurse(i+1, inputFd);
                munmapSharedMems();
                _exit(EXIT_SUCCESS);
                break;
            default:
                shmChildPids[index++] = childPid;
                break;
        }
    }
    close(inputFd);

    for(i=0; i<numOfVaccinator; ++i){
        switch (childPid=fork())
        {
            case 0:
                // setpgid(0, getppid()); // put all children in a group to send collective signal
                adjustHandler(SIGINT, handlerIntChild);
                vaccinator(i+1);
                munmapSharedMems();
                _exit(EXIT_SUCCESS);
                break;
            default:
                shmChildPids[index++] = childPid;
                break;
        }
    }

    // fork pusherV1
    switch (childPid=fork())
    {
        case 0:
            // setpgid(0, getppid()); // put all children in a group to send collective signal
            adjustHandler(SIGINT, handlerIntChild);
            pusherV1(numOfCitizens*t);
            munmapSharedMems();
            _exit(EXIT_SUCCESS);
            break;        
        default:
            shmChildPids[index++] = childPid;
            break;
    }

    pusherV2(numOfCitizens*t);


    waitChildren();

    fprintf(stderr, "The clinic is now closed. Stay healthy.\n");

    destroyAllSemaphores();
    munmapSharedMems();
    unlinkSharedMems();
    return 0;
}

void
waitChildren(){
    for(int i=0; i<(numOfChildren); ++i){
        wait(NULL);
    }
}

void 
munmapSharedMems(){
    munmap(shmStatic, sizeof(struct ShmStatic));
    munmap(shmBuff,  capacityOfBuffer*sizeof(char));
    munmap(shmCitizens,  numOfCitizens*sizeof(struct Citizen));
    munmap(shmChildPids,  numOfChildren*sizeof(int));
}

void
unlinkSharedMems(){
    shm_unlink(SHM_CITIZENS_NAME);
    shm_unlink(SHM_BUFF_NAME);
    shm_unlink(SHM_STATIC_NAME);
    shm_unlink(SHM_CHILD_IDS);
}

void 
destroyAllSemaphores(){
    sem_destroy(&(shmStatic->semEmpty));
    sem_destroy(&(shmStatic->semCitizenAvailable));
    sem_destroy(&(shmStatic->shmLock));
    sem_destroy(&(shmStatic->semVacc1));
    sem_destroy(&(shmStatic->semVacc2));
    sem_destroy(&(shmStatic->semVaccAvailable));
}

int 
constituteSharedMem(int bufferCapacity, int numOfNurses, int numOfCitizen, int t){
 
    // SHARED MEMORY for static vars and unnamed sems
    int fdShmStatic = shm_open(SHM_STATIC_NAME, O_RDWR|O_CREAT, S_IRUSR | S_IWUSR);
    if(fdShmStatic == -1){
        perror("shm_open error");
        return -1;
    }

    if(ftruncate(fdShmStatic, sizeof(struct ShmStatic)) == -1){
        perror("ftruncate error");
        return -1;
    }

    shmStatic = (struct ShmStatic*)mmap(NULL, sizeof(struct ShmStatic), PROT_READ | PROT_WRITE, MAP_SHARED, fdShmStatic, 0);
    if(shmStatic == MAP_FAILED){
        perror("mmap error");
        return -1;
    }
    close(fdShmStatic);

    // SHARED MEMORY BUFF
    int fdBuff = shm_open(SHM_BUFF_NAME, O_RDWR|O_CREAT, S_IRUSR | S_IWUSR);
    if(fdBuff == -1){
        perror("shm_open error");
        return -1;
    }

    if(ftruncate(fdBuff, sizeof(char)*bufferCapacity) == -1){
        perror("ftruncate error");
        return -1;
    }

    shmBuff = (char*)mmap(NULL, sizeof(char)*bufferCapacity, PROT_READ | PROT_WRITE, MAP_SHARED, fdBuff, 0);
    if(shmBuff == MAP_FAILED){
        perror("mmap error");
        return -1;
    }

    for(int i=0; i<bufferCapacity; ++i){
        shmBuff[i] = '0';
    }
    close(fdBuff);

    // SHARED MEMORY CHILDREN IDS
    int fdChildren = shm_open(SHM_CHILD_IDS, O_RDWR|O_CREAT, S_IRUSR | S_IWUSR);
    if(fdChildren == -1){
        perror("shm_open error");
        return -1;
    }

    if(ftruncate(fdChildren, sizeof(int)*numOfChildren) == -1){
        perror("ftruncate error");
        return -1;
    }

    shmChildPids = (int*)mmap(NULL, sizeof(int)*numOfChildren, PROT_READ | PROT_WRITE, MAP_SHARED, fdChildren, 0);
    if(shmChildPids == MAP_FAILED){
        perror("mmap error");
        return -1;
    }

    for(int i=0; i<numOfChildren; ++i){
        shmBuff[i] = '0';
    }
    close(fdChildren);

    // SHARED MEMORY CITIZENS
    int fdCitizens = shm_open(SHM_CITIZENS_NAME, O_RDWR|O_CREAT, S_IRUSR | S_IWUSR);
    if(fdCitizens == -1){
        perror("shm_open error");
        return -1;
    }

    if(ftruncate(fdCitizens, sizeof(struct Citizen)*numOfCitizen) == -1){
        perror("ftruncate error");
        return -1;
    }

    shmCitizens = (struct Citizen*)mmap(NULL, sizeof(struct Citizen)*numOfCitizen, PROT_READ | PROT_WRITE, MAP_SHARED, fdCitizens, 0);
    if(shmCitizens == MAP_FAILED){
        perror("mmap error");
        return -1;
    }
    
    for(int i=0; i<numOfCitizen; ++i){
        shmCitizens[i].inClinic = 0;
        shmCitizens[i].gone = 0;
    }
    close(fdCitizens);

    if(sem_init(&(shmStatic->semEmpty), 1, bufferCapacity) == -1)           { perror("sem_init error"); return -1; }
    if(sem_init(&(shmStatic->semCitizenAvailable), 1, numOfCitizen) == -1)  { perror("sem_init error"); return -1; }
    if(sem_init(&(shmStatic->shmLock), 1, 1) == -1)             { perror("sem_init error"); return -1; }
    if(sem_init(&(shmStatic->semVacc1), 1, 0) == -1)            { perror("sem_init error"); return -1; }
    if(sem_init(&(shmStatic->semVacc2), 1, 0) == -1)            { perror("sem_init error"); return -1; }
    if(sem_init(&(shmStatic->semVaccAvailable), 1, 0) == -1)    { perror("sem_init error"); return -1; }

    shmStatic->buffSize = 0;
    shmStatic->buffCapacity = bufferCapacity;
    shmStatic->leftCitizens = numOfCitizen;
    shmStatic->totalNumOfCitizens = numOfCitizen;
    shmStatic->terminate = 0;
    shmStatic->leftNurses = numOfNurses;
    shmStatic->vaccinationSession = numOfCitizen*t;

    return 0;
}

int 
citizen(int citizenNo, int t, sigset_t sigsetSusUsr1){

    char message[CITIZEN_MSG_LEN];
    char *msgRound = "Citizen %d (pid=%d) is vaccinated for the %d time: the clinic has %d vaccine1 and %d vaccine2\n";
    char *msgTerm = "Citizen %d (pid=%d) is vaccinated for the %d time: the clinic has %d vaccine1 and %d vaccine2. The citizen is leaving. Remaining citizens to vaccinate: %d\n";
  
    int counter= 0, citizenIndex;
    while(t>0){
        ++counter;
        --t;

        sigsuspend(&sigsetSusUsr1);
        sem_wait(&(shmStatic->shmLock));

        citizenIndex = getCitizenIndex(getpid());

        if(t == 0){
             sprintf(message, msgTerm, 
                citizenNo, 
                getpid(),
                counter,
                countOf('1'),
                countOf('2'),
                --(shmStatic->leftCitizens)
            );
            shmCitizens[citizenIndex].gone = 1;
        }
        else{
             sprintf(message, msgRound, 
                citizenNo, 
                getpid(),
                counter,
                countOf('1'),
                countOf('2')
            );
            sem_post(&(shmStatic->semCitizenAvailable));
        }
        fprintf(stderr, "%s", message);
        fflush(stderr);

        if(shmStatic->leftCitizens == 0){
            fprintf(stderr, "All citizens have been vaccinated.\n");
        }
        fflush(stderr);

        shmCitizens[citizenIndex].inClinic = 0;
        sem_post(&(shmStatic->shmLock));
    }
    return 0;
}

int 
vaccinator(int vaccinatorNo){
    
    char cont=1;
    int  citizenPid=0, numOfDoses=0, citizenIndex=0;
    while(cont){      

        sem_wait(&(shmStatic->shmLock)); 
        if((shmStatic->vaccinationSession) > 0){
            --(shmStatic->vaccinationSession);
            sem_post(&(shmStatic->shmLock));

            sem_wait(&(shmStatic->semVaccAvailable));
            sem_wait(&(shmStatic->semCitizenAvailable));
            sem_wait(&(shmStatic->shmLock)); 
            citizenPid = getOldestAvailableCitizenPid();

            if(citizenPid == 0){
                fprintf(stderr, "[ERROR] ====>>>> citizenPid is 0.\n");
            }

            removeChar(shmStatic->buffCapacity, '1');
            removeChar(shmStatic->buffCapacity, '2');

            citizenIndex = getCitizenIndex(citizenPid);
            shmCitizens[citizenIndex].inClinic = 1;
            sem_post(&(shmStatic->shmLock));            
            kill(citizenPid, SIGUSR1);
            fprintf(stderr,"Vaccinator %d (pid=%d) is inviting citizen pid=%d to the clinic.\n",
                            vaccinatorNo,
                            getpid(),
                            citizenPid
            );
            fflush(stderr);
            ++numOfDoses;
            sem_post(&(shmStatic->semEmpty));
            sem_post(&(shmStatic->semEmpty));
           
            sem_wait(&(shmStatic->shmLock));            
            if(shmStatic->vaccinationSession == 0){
                fprintf(stderr, "Vaccinator %d (pid=%d) vaccinated %d doses.\n",
                                vaccinatorNo,
                                getpid(),
                                numOfDoses
                );
                fflush(stderr);
            }
            sem_post(&(shmStatic->shmLock));            

        }
        else{
            sem_post(&(shmStatic->shmLock));
            cont = 0;
        }
    }

    return 0;
}

int 
removeChar(int capacity, char c){

    int i=0;
    while(i<capacity && shmBuff[i] != c){
        ++i;
    }
    shmBuff[i]= '0';
    shmStatic->buffSize = (shmStatic->buffSize)-1;

    return 0;
}

int 
nurse(int nurseNo, int fd){

    char vaccine,
         cont= 1;
    int numOfBytes,
        i;

    while(cont){

        flock(fd, LOCK_EX);
        numOfBytes = read(fd, &vaccine, sizeof(char));
        
        if(numOfBytes == 0){
            cont = 0;
            sem_wait(&(shmStatic->shmLock));
            --(shmStatic->leftNurses);
            if((shmStatic->leftNurses)==0){
                fprintf(stderr, "Nurses have carried all vaccines to the buffer, terminating.\n");
                fflush(stderr);
            }
            sem_post(&(shmStatic->shmLock));
        }
        else if(numOfBytes == -1){
            perror("read error");
            sem_wait(&(shmStatic->shmLock));
            shmStatic->terminate = 1;
            sem_post(&(shmStatic->shmLock));
            
            flock(fd, LOCK_UN);
            return -1;
        }
        else {

            sem_wait(&(shmStatic->semEmpty));
            sem_wait(&(shmStatic->shmLock));
            i = findFirstEmptyIndex();
            shmBuff[i] = vaccine;
            shmStatic->buffSize = (shmStatic->buffSize)+1;

            if(vaccine == '1' || vaccine == '2'){ // eliminate newLine or whitespaces or any unknown character
                fprintf(stderr,"Nurse %d (pid=%d) has brought vaccine %c: the clinic has %d vaccine1 and %d vaccine2.\n",
                                nurseNo,
                                getpid(),
                                vaccine,
                                countOf('1'),
                                countOf('2')
                );
                fflush(stderr);
            }

            sem_post(&(shmStatic->shmLock));    

            if(vaccine == '1'){
                sem_post(&(shmStatic->semVacc1));
            }
            else if(vaccine == '2'){
                sem_post(&(shmStatic->semVacc2));
            }                                                    
        }
        flock(fd, LOCK_UN);
    }
    return 0;
}

int 
findFirstEmptyIndex(){

    int i=0;
    while(i<shmStatic->buffCapacity && shmBuff[i]!='0'){ 
        ++i;
    }
    return i;
}

int 
pusherV1(int numOfV1){
    
    char cont=1;
    while(numOfV1>0 && cont){
        --numOfV1;

        sem_wait(&(shmStatic->semVacc1));
        sem_wait(&(shmStatic->shmLock));
        if(isExist('2')){
            sem_post(&(shmStatic->semVaccAvailable));
        }
        sem_post(&(shmStatic->shmLock));
    }        
    return 0;
}

int 
pusherV2(int numOfV2){
        
    char cont=1;
    while(numOfV2>0 && cont){ 
        --numOfV2;

        sem_wait(&(shmStatic->semVacc2));
        sem_wait(&(shmStatic->shmLock));
        if(isExist('1')){
            sem_post(&(shmStatic->semVaccAvailable));
        }
        sem_post(&(shmStatic->shmLock));
    }

    return 0;
}

int 
isExist(char c){

    int i=0;
    while(i<(shmStatic->buffCapacity) && shmBuff[i]!=c){ ++i; }
    
    return (i < (shmStatic->buffCapacity) );
}

int 
countOf(char c){
    int count =0;
    for(int i=0; i<(shmStatic->buffCapacity); ++i){
        if(shmBuff[i] == c){
            ++count;
        }
    }
    return count;
}

int adjustHandler(int sigNo, void handler(int)){
    
    struct sigaction sact;
    sact.sa_flags = 0;
    sact.sa_handler = handler;

    if(sigemptyset(&sact.sa_mask) == -1 || sigaction(sigNo, &sact, NULL) == -1){
        perror("sigaction() error");
        return -1;
    }
    return 0;
}

int 
getOldestAvailableCitizenPid(){
    
    int i=0;
    while(i<(shmStatic->totalNumOfCitizens) && ((shmCitizens[i].gone) || (shmCitizens[i].inClinic))){
        ++i;
    }
    if(i == (shmStatic->totalNumOfCitizens)){
        return 0;
    }
    return shmCitizens[i].pid;
}   

int 
getCitizenIndex(int pid){

    int i = 0;
    while(i < (shmStatic->totalNumOfCitizens) && shmCitizens[i].pid != pid){ ++i; }

    return i;
}

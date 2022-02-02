#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <semaphore.h>
#include <pthread.h>
#include <unistd.h>
#include <signal.h>

/// MACROS ///////////////////////////////////////////
#define NAME_MAX_LEN 255
#define SLEEP_MAX 6

enum TermStatus{ working='1', hwDone='D', noMoney='$', keyboardInt='T' };

/// GLOBAL VARIABLES ////////////////////////////////
pthread_t tHwOwner;
int avilableMoney=0;
char *hwQueue=NULL;
int currHw=0;
struct WorkerInfo* workers;
int numOfWorkers=0; 

enum TermStatus termStatus = working;

sem_t semQueueReady;
sem_t semLock;
sem_t semEnd;
sem_t semAvailableWorkers;

sig_atomic_t sigInt=0;

/// STRUCTS /////////////////////////////////////////
struct ThreadInfo{
    pthread_t thread_id;
    int thread_num;  
};

struct WorkerInfo{
    char name[NAME_MAX_LEN];
    int quality,
        speed, 
        price,
        studiedHw;
    char doingHw,
         bascet;
    sem_t semCall;
};

/// FUNCTION PROTOTYPES ////////////////////////////
void* homeworkOwner(void*);
void* worker(void*);
int getWorkerStudents(int stFd);    
int initSems();
int organizeThreads();
int getMostQualifiedIndex();
int getFasterIndex();
int getCheapestIndex();
int isMoneyEnough();
int isEffortableAvailable();


// signal handlers
void 
handler(int sigNo){
    ++sigInt;
}

int
main(int argc, char **argv){

    if(argc<4 || strcmp(argv[1], "--help") == 0){
        fprintf(stderr, "Usage: %s homeworkFilePath studentsFilePath availableMoney\n", argv[0]);
    }

    struct sigaction sact;
    if(sigemptyset(&sact.sa_mask) != 0){
        perror("sigemptyset() error");
        return -1;
    }
    sact.sa_flags = 0;
    sact.sa_handler = handler;
    if(sigaction(SIGINT, &sact, NULL) != 0){
        perror("sigaction() error");
        return -1;
    }

    // get command line arguments 
    char *hwFilename = argv[1];
    char *stFilename = argv[2];
    avilableMoney = atoi(argv[3]);

    // open files 
    int stFd, hwFd;
    stFd = open(stFilename, O_RDONLY);
    hwFd = open(hwFilename, O_RDONLY);
    if(stFd == -1 || hwFd == -1){
        perror("open error");
        return -1;
    }

    numOfWorkers = getWorkerStudents(stFd);
    initSems();
    
    // create threads
    struct ThreadInfo tinfoArr[numOfWorkers];

    if(pthread_create(&tHwOwner, NULL, homeworkOwner, &hwFd)){
        close(stFd);
        close(hwFd);
        fprintf(stderr, "pthread_create error\n");
        return -1;
    }
    currHw=0;
    sem_wait(&semQueueReady); // wait for the homework queue is ready 
    
    // fprintf(stderr, "QUEUE is ready!!!!!!!!!!!\n");
    for(int i=0; i<numOfWorkers; i++){
        tinfoArr[i].thread_num = i;
        pthread_create(&tinfoArr[i].thread_id, NULL, worker, &tinfoArr[i].thread_num);
    }

    // organize threads
    organizeThreads();

    // wait for threads
    for(int i=0; i<numOfWorkers; ++i)
        pthread_join(tinfoArr[i].thread_id, NULL);

    fprintf(stderr, "Homeworks solved and money made by the students:\n");
    int totalCost=0;
    int totalHwStudied=0;
    for(int i=0; i<numOfWorkers; ++i){
        fprintf(stderr, "%s %d %d\n", workers[i].name, workers[i].studiedHw, workers[i].studiedHw * workers[i].price);
        totalCost+= workers[i].studiedHw * workers[i].price;
        totalHwStudied+= workers[i].studiedHw;
    }
    fprintf(stderr, "Total cost for %d homeworks %dTL\n", totalHwStudied, totalCost);
    fprintf(stderr, "Money left at H’s account: %dTL\n", avilableMoney);

    sem_destroy(&semQueueReady);
    sem_destroy(&semLock);
    sem_destroy(&semEnd);
    sem_destroy(&semAvailableWorkers);
    for(int i=0; i<numOfWorkers; ++i){ 
        sem_destroy(&(workers[i].semCall));
    }
    free(workers);
    free(hwQueue);
    close(stFd);
    close(hwFd);

    return 0;
}

int 
organizeThreads(){

    char cont=1;
    int workerIndex=0;

    while(cont==1 && termStatus==working && currHw<strlen(hwQueue)){
        sem_wait(&semAvailableWorkers);
        if(isMoneyEnough() == 0){
            fprintf(stderr, "Money is over, closing.\n");
            sem_wait(&semLock);
            for(int i=0; i<numOfWorkers; ++i){
                workers[i].bascet = noMoney;
                sem_post(&(workers[i].semCall));
            }
            termStatus = noMoney;
            sem_post(&semEnd);
            sem_post(&semLock);
            cont=0;
            continue;
        }
        else if(isEffortableAvailable()==0){
            continue;
        }

        if(sigInt>0){
            fprintf(stderr, "Termination signal received, closing.\n");
            sem_wait(&semLock);
            for(int i=0; i<numOfWorkers; ++i){
                workers[i].bascet = keyboardInt;
                sem_post(&(workers[i].semCall));
            }
            termStatus = keyboardInt;
            sem_post(&semEnd);
            sem_post(&semLock);
            cont=0;
            continue;
        }

        switch(hwQueue[currHw]){
            case 'Q': //find most qualified
                workerIndex = getMostQualifiedIndex();
                if(workerIndex == -1){
                    fprintf(stderr, "getMostQualifiedIndex() returned -1\n");
                    cont = 0;
                    continue;
                }
                
                sem_wait(&semLock);
                workers[workerIndex].bascet = hwQueue[currHw];
                ++currHw;
                sem_post(&(workers[workerIndex].semCall));
                sem_post(&semLock);
                break;
            case 'S': //find faster
                workerIndex = getFasterIndex();
                if(workerIndex == -1){
                    fprintf(stderr, "getFasterIndex() returned -1\n");
                    cont = 0;
                    continue;
                }
                
                sem_wait(&semLock);
                workers[workerIndex].bascet = hwQueue[currHw];
                ++currHw;
                sem_post(&(workers[workerIndex].semCall));
                sem_post(&semLock);
                break;

            case 'C': // find cheapest
                workerIndex = getCheapestIndex();
                if(workerIndex == -1){
                    fprintf(stderr, "getCheapestIndex() returned -1\n");
                    cont = 0;
                    continue;
                }
                
                sem_wait(&semLock);
                workers[workerIndex].bascet = hwQueue[currHw];
                ++currHw;
                sem_post(&(workers[workerIndex].semCall));
                sem_post(&semLock);
                break;
        }
    }

    if(sigInt==0 && currHw == strlen(hwQueue)){
        fprintf(stderr, "No more homeworks left or coming in, closing.\n");
        sem_wait(&semLock);
        for(int i=0; i<numOfWorkers; ++i){
            workers[i].bascet = hwDone;
            sem_post(&(workers[i].semCall));
        }
        termStatus = hwDone;
        sem_post(&semEnd);
        sem_post(&semLock);
    }
    return 0;
}

int 
isEffortableAvailable(){

    char effortableAvailable=0;
    for(int i=0; i<numOfWorkers; ++i){
        if(avilableMoney >= workers[i].price && workers[i].doingHw == 0){
            effortableAvailable=1;
        }
    }

    return effortableAvailable;
}

int 
isMoneyEnough(){
    
    int minPrice =  workers[0].price;
    for(int i=1; i<numOfWorkers; ++i)
        if(minPrice >  workers[i].price)
            minPrice = workers[i].price;
        
    
    return (avilableMoney > minPrice);
}

int getMostQualifiedIndex(){

    int index=-1;
    sem_wait(&semLock);
    for(int i=0; i<numOfWorkers; ++i){
       if(index == -1){
            if( workers[i].doingHw == 0 && avilableMoney >= workers[i].price)
            {
                index = i;
            }
        }
        else{
            if( workers[i].doingHw == 0 && 
                (workers[i].quality > workers[index].quality && avilableMoney >= workers[i].price) )
            {
                index = i;
            }
        }
    }
    sem_post(&semLock);
    return index;
}

int getFasterIndex(){

    int index=-1;
    sem_wait(&semLock);
    for(int i=0; i<numOfWorkers; ++i){
        if(index == -1){
            if( workers[i].doingHw == 0 && avilableMoney >= workers[i].price)
            {
                index = i;
            }
        }
        else{
            if( workers[i].doingHw == 0 && 
                (workers[i].speed > workers[index].speed && avilableMoney >= workers[i].price) )
            {
                index = i;
            }
        }
        
    }
    sem_post(&semLock);
    return index;
}

int getCheapestIndex(){
    
    int index =0;
    sem_wait(&semLock);
    for(int i=1; i<numOfWorkers; ++i){
       if(index == -1){
            if( workers[i].doingHw == 0 && avilableMoney >= workers[i].price)
            {
                index = i;
            }
        }
        else{
            if( workers[i].doingHw == 0 && 
                (workers[i].price < workers[index].price && avilableMoney >= workers[i].price) )
            {
                index = i;
            }
        }
    }
    
    sem_post(&semLock);
    return index;
}

int 
initSems(){

    sem_init(&semQueueReady, 0, 0);
    sem_init(&semLock, 0, 1);
    sem_init(&semAvailableWorkers, 0, numOfWorkers);
    for(int i=0; i<numOfWorkers; ++i)
        sem_init(&(workers[i].semCall), 0, 0);

    return 0;
}

int 
getWorkerStudents(int stFd){

    // count lines 
    FILE *fp = fdopen(stFd, "r");

    int lineCount=0;
    char c;
    while ((c= fgetc(fp))!= EOF){ 
        if(c=='\n')
            ++lineCount; 
    }
    ++lineCount; 

    rewind(fp);    // rewind cursor

    // allocate array
    workers = (struct WorkerInfo*)malloc(sizeof(struct WorkerInfo)*lineCount);
    
    for(int i=0; i<lineCount; ++i){
        fscanf(fp, "%s %d %d %d\n", workers[i].name, &(workers[i].quality), &(workers[i].speed), &(workers[i].price)); 
        workers[i].doingHw=0;
        workers[i].studiedHw=0;
    }
    fclose(fp);
    return lineCount;   // return num of workers
}

void* 
homeworkOwner(void *args){

    int fd = *((int*)args);
    // fprintf(stderr, "input file name for hw: %d\n", fd);

    // count all assignments
    FILE *fp = fdopen(fd, "r");
    int count=0;
    while (fgetc(fp)!= EOF){ ++count; }
    rewind(fp);    // rewind cursor

    // allocate space for queue
    hwQueue = (char*)malloc(sizeof(char)*(1+count));

    // read file and fill the queue
    char c;
    int i=0;
    while((c=fgetc(fp))!=EOF){
        fprintf(stderr, "H has a new homework %c; remaining money is 10000TL\n", c);
        hwQueue[i++]=c;
    }
    hwQueue[i]=0;
    fclose(fp);


    // sempost hwQueue is ready
    sem_post(&semQueueReady);

    sem_wait(&semEnd);
    sem_wait(&semLock);
    switch(termStatus){
        case noMoney:
            fprintf(stderr, "H has no more money for homeworks, terminating.\n");
            break;
        case hwDone:
            fprintf(stderr, "H has no other homeworks, terminating.\n");
            break;
        case keyboardInt:
            fprintf(stderr, "Keyboard interupt CTRL-C, H is being terminated.\n");
            break;
        default:    break;
    }
    sem_post(&semLock);
    // detach itself
    pthread_detach(tHwOwner);
    pthread_exit((void*)0);
}

void*
worker(void *args){
    
    int i = *(int*)args;

    char cont = 1;
    while(cont==1){

        fprintf(stderr, "%s is waiting for homework\n", workers[i].name);

        sem_wait(&(workers[i].semCall));
        
        sem_wait(&semLock);
        if(workers[i].bascet == noMoney || workers[i].bascet == hwDone || workers[i].bascet == keyboardInt || workers[i].price > avilableMoney){
            sem_post(&semLock);
            cont = 0;
            continue;
        }
        
        workers[i].doingHw = 1;
        avilableMoney -= (workers[i].price);
        fprintf(stderr, "%s doing homework %c for %d, H has %dTL left.\n",
                workers[i].name,
                workers[i].bascet, 
                workers[i].price,
                avilableMoney);

        sem_post(&semLock);
        
        sleep(SLEEP_MAX - workers[i].speed);
        
        sem_wait(&semLock);
        workers[i].doingHw = 0;
        ++(workers[i].studiedHw);
        sem_post(&semAvailableWorkers);
        sem_post(&semLock);
    }

    pthread_exit((void*)0);
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <semaphore.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <fcntl.h>

//***** USER DEFINED LIBS
#include "io_helper.h"
#include "supplier.h"
#include "cook.h"
#include "student.h"
#include "base.h"

/////////////////////////// FUNCTION PROTOTYPES ////////////////////////////////
void clean_resources();
    // cleans program resources
int retrieve_integer(char* optarg, char* label);
    // retrieves int value from optarg, label is used in case of error to print message
void start_processes(char* filename, shMem* mem);
    // sets up shared memory, semaphores, forks etc. and manages the main process
void allocate_shared_mem(shMem** mem, char* sh_name);
    // allocates shared memory
void init_sems(shMem* mem);
    // initialize semaphores which are located in shared memory
void destroy_sems(shMem* mem);
    // destroy semephores
void fork_and_wait(char* filename, shMem* mem);
    // forks and wait for children


////////////////////////// IMPLEMENTATION BEGINS ///////////////////////////////
int
main(int argc, char *argv[]){

    shMem* mem = NULL;
    char* filename;

    atexit(clean_resources);

    if(argc != 15){
        print_usage(argv[0]);
        exit(1);
    }

    int option;
    while((option = getopt(argc, argv, "N:T:S:L:U:G:F:")) != -1){
        switch(option){
            case 'N':
                N = retrieve_integer(optarg, "Number of cooks");
                if(N <= 2)
                    err_exit("N(num of cooks) must be greater than 2\n");
                break;
            case 'T':
                T = retrieve_integer(optarg, "Number of tables");
                if(T < 1)
                    err_exit("T(num of tables) must be greater than 0\n");
                break;
            case 'S':
                S = retrieve_integer(optarg, "Size of counter");
                if(S <= 3)
                    err_exit("S(size of counter) must be greater than 3\n");
                break;
            case 'L':
                L = retrieve_integer(optarg, "Number of loops for students");
                if(L < 3)
                    err_exit("L(num of loops for students) must be at least 3\n");
                break;
            case 'U':
                U = retrieve_integer(optarg, "Number of undergraduated students");
                break;
            case 'G':
                G = retrieve_integer(optarg, "Number of graduated students");
                M = U + G;
                if(G < 1 || U <= G || U+G <= 3 )
                    err_exit("U and G restricted as fallows : M=U+G > 3, U>G>=1\n");
                if(M <= N || M <= T)
                    err_exit("M(total num of students) must be greater than num of cooks and num of tables\n");
                break;
            case 'F':
                filename = optarg;
                break;
            default:
                print_usage(argv[0]);
                exit(1);
        }
    }

    K = 2*L*M+1;
    start_processes(filename, mem);

    return 0;
}

void
handler(int signum){
    err_exit("The process is exiting with a SIGINT signal\n");
}

void
start_processes(char* filename, shMem* mem){

    struct sigaction sact;
    memset(&sact, 0, sizeof(sact));
    sact.sa_handler = &handler;
    sigaction(SIGINT, &sact, NULL);

    char* sh_name = "share";

    allocate_shared_mem(&mem, sh_name);
    init_sems(mem);
    fork_and_wait(filename, mem);
    shm_unlink(sh_name);
}

void
allocate_shared_mem(shMem** mem, char* sh_name){

    int shmFd = shm_open(sh_name, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);

    if(shmFd == -1)
        err_exit("Shared memory could not be allocated.\n");

    if(ftruncate(shmFd, sizeof(shMem)) == -1)
        err_exit("ftruncate failed.\n");

    *mem = (shMem*)mmap(NULL, sizeof(shMem), PROT_READ | PROT_WRITE, MAP_SHARED, shmFd, 0);

    if(mem == MAP_FAILED)
        err_exit("mmap is failed.\n");

}

void
init_sems(shMem* mem){

    // semaphores to manage kitchen
    sem_init(&(mem->kSoup), 1, 0);
    sem_init(&(mem->kMainCourse), 1, 0);
    sem_init(&(mem->kDessert), 1, 0);
    sem_init(&(mem->kTotalPlate), 1, (2*M*L+1));

    // to manage meal counter
    sem_init(&(mem->trays), 1, 0);
    sem_init(&(mem->counterRooms), 1, (S/3));

    // to manage synchronization btw students
    sem_init(&(mem->fTables), 1, T);
    sem_init(&(mem->tryTakeTray), 1, 1);

    sem_init(&(mem->mTray), 1, 1);
    sem_init(&(mem->mCook), 1, 1);
    sem_init(&(mem->mTryCook), 1, 1);
    sem_init(&(mem->mSoup), 1, 1);
    sem_init(&(mem->mMainC), 1, 1);
    sem_init(&(mem->mDessert), 1, 1);
    sem_init(&(mem->mSupply), 1, 1);
    sem_init(&(mem->mPlate), 1, 1);

    sem_init(&(mem->mTb), 1, 1);
    sem_init(&(mem->mSt), 1, 1);
    sem_init(&(mem->mStTry), 1, 1);

}

void
destroy_sems(shMem* mem){

    sem_destroy(&(mem->kSoup));
    sem_destroy(&(mem->kMainCourse));
    sem_destroy(&(mem->kDessert));
    sem_destroy(&(mem->kTotalPlate));
    sem_destroy(&(mem->trays));
    sem_destroy(&(mem->counterRooms));
    sem_destroy(&(mem->fTables));
    sem_destroy(&(mem->tryTakeTray));
    sem_destroy(&(mem->mTray));
    sem_destroy(&(mem->mCook));
    sem_destroy(&(mem->mTryCook));
    sem_destroy(&(mem->mSoup));
    sem_destroy(&(mem->mMainC));
    sem_destroy(&(mem->mDessert));
    sem_destroy(&(mem->mSupply));
    sem_destroy(&(mem->mPlate));
    sem_destroy(&(mem->mTb));
    sem_destroy(&(mem->mSt));
    sem_destroy(&(mem->mStTry));

}


void
fork_and_wait(char* filename, shMem* mem) {

    int i;

    mem->supply = true;    // information about supplying. 1 is on 0 is off
    mem->trayCounts =0; // number of trays on the counter (1 tray = [P, C, D])
    mem->onSoup = false;
    mem->onMainC = false;
    mem->onDessert = false;
    mem->cUndSt = U;
    mem->cGradSt = G;
    mem->cFreeTb = T;

    // for supplier
    if(fork() == 0){
        supply(filename, mem);
        return;
    }

    // for cooks
    for(i=0; i<N; ++i){
        if(fork()==0){
            work_as_cook(mem, i);
            return;
        }
    }

    // for und students
    for(i=0; i<U+G; ++i){
        if(fork()==0){
            eat_as_und(mem, i);
            return;
        }
    }

    for(i=0; i<(N+U+G+1); ++i){
        wait(NULL);
    }

    destroy_sems(mem);
    // shm_unlink(sh_name);
}

int
retrieve_integer(char* optarg, char* label){

    char* endPtr = NULL;
    int result = strtol(optarg, &endPtr, BASE);

    // error control
    if(optarg == endPtr){ // in this case the string could not be converted
        char* template = "%s is not valid.\n";
        int msgSize = strlen(label)+strlen(template)-2;
        char errMsg[msgSize];
        snprintf(errMsg, msgSize, template, label);
        err_exit(errMsg);
    }

    return result;
}

void
clean_resources(){

}

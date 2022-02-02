#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/fcntl.h>

// USER DEFINED LIBS
#include "base.h"

///////////////////////////////// MACROS ///////////////////////////////////////
#define TERMINATE_CHEF -1
#define PROCESS_NAME_LEN 25

/////////////////////////// GLOBAL VARIABLES ///////////////////////////////////
int fd;
Sems sems;

// wholesaler modify new_ings according to what s/he brings. 
// ( *new_ings == -1(TERMINATE_CHEF), tells that delivery is done)
int *new_ings; 

/////////////////////////// FUNCTION PROTOTYPES ////////////////////////////////
int start_work();
void init_sems(Sems* sems);
void destroy_sems(Sems* sems);
void* chef_thread_func(void* args);
void detect_absence_ings(int inf_ings, char** ingr1, char** ingr2);
int wholesaler_func();
void print_usage(char *process_name); // promt operations
void err_exit(char* error_msg); // print the message to the stderr end terminate program


////////////////////////// IMPLEMENTATION BEGINS ///////////////////////////////
int
main(int argc, char *argv[]){

    srand(time(NULL));

    if(argc != 3 ) {
        print_usage(argv[0]);
        exit(EXIT_FAILURE);
    }

    int option;
    while((option = getopt(argc, argv, "-i:")) != -1){
        switch(option){
            case 'i':
                fd = open(optarg, O_RDONLY);
                break;
            default:
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    if(fd == -1 )
        err_exit("The file could not be opened successfully.\n");

    start_work();
    close(fd);

    return 0;
}

int
start_work(){

    init_sems(&sems);
    
    int chf_num = 6;
    int chef_inf_ings[6] = {3, 5, 9, 6, 10, 12}; // according to binary format and 'ingredients' enum
    pthread_t chefs [chf_num];
    thArg* arg_arr[chf_num]; // pthread_create arguments
    int s; // keeps fucntion return vals to check error

    // allocations
    new_ings = (int*)malloc(sizeof(int));
    for(int i=0; i<chf_num; ++i)
        arg_arr[i] = (thArg*)malloc(sizeof(thArg));   

    char *msg_err_create = "Thread %d could not be created.\n";
    char *msg_err_join = "pthread_join to thread %d is failed.\n";
    int msg_len = strlen(msg_err_join)+11; //longest temp msg + max int len + NULL char 
    char msg[msg_len];

    // thread creation 
    for(int i=0; i<chf_num; ++i){

        (arg_arr[i])->chf_ings = chef_inf_ings[i];
        (arg_arr[i])->chf_index = i+1;
        s = pthread_create(&(chefs[i]), NULL, chef_thread_func, arg_arr[i]);
        if(s!=0){ 
            snprintf(msg, msg_len, msg_err_create, i+1);
            err_exit(msg);
        }
    }

    // wholesaler reads input file and deliver the ingredients to corresponding chefs.
    // After the input file content is consumed entirely, the chef threads are notified to be finished.
    wholesaler_func(); 

    // thread joining
    for(int i=0; i<chf_num; ++i){
        s = pthread_join(chefs[i], NULL);
        if(s!=0){
            snprintf(msg, msg_len, msg_err_join, i+1);
            err_exit(msg);
        }
    }

    destroy_sems(&(sems));

    free(new_ings);
    for(int i=0; i<chf_num; ++i)
        free(arg_arr[i]);

    return 0;
}

void* 
chef_thread_func(void* arg){    

    thArg* args = (thArg*)(arg);

    int inf_ings = args->chf_ings,
        chf_index = args->chf_index;

    char *wait_temp = "chef %d is waiting for %s and %s\n";
    char *take_temp = "chef %d has taken %s\n";
    char *prep_temp = "chef %d is preparing the dessert\n";
    char *deliver_temp = "chef %d has delivered the dessert to the wholesaler\n";

    int msg_len = strlen(deliver_temp)+30;
    char msg[msg_len];
    char *ingr1, *ingr2;
    detect_absence_ings(inf_ings, &ingr1, &ingr2);

    snprintf(msg, msg_len, wait_temp, chf_index, ingr1, ingr2);
    write(STDERR_FILENO, msg, strlen(msg));
    while(1){
       
        sem_wait(&(sems.ingr_ready));
        sem_wait(&(sems.lock_ingr));
        if((*new_ings)== TERMINATE_CHEF){
            // delivery is done
            sem_post(&(sems.ingr_ready)); 
            sem_post(&(sems.lock_ingr));
            return NULL;
        }

        if((inf_ings + *new_ings) == (M | F | W | S)){

            snprintf(msg, msg_len, take_temp, chf_index, ingr1);
            write(STDERR_FILENO, msg, strlen(msg));
            
            snprintf(msg, msg_len, take_temp, chf_index, ingr2);
            write(STDERR_FILENO, msg, strlen(msg));

            *new_ings = 0;

            snprintf(msg, msg_len, prep_temp, chf_index); 
            write(STDERR_FILENO, msg, strlen(msg));

            sleep((rand()%5)+1); //simulates process of making güllaç
            sem_post(&(sems.dess_ready));

            snprintf(msg, msg_len, deliver_temp, chf_index);
            write(STDERR_FILENO, msg, strlen(msg));

            snprintf(msg, msg_len, wait_temp, chf_index, ingr1, ingr2);
            write(STDERR_FILENO, msg, strlen(msg));
        }
        else if(*new_ings>0) {
            // if new ingredients do not complate our recipe we let other chefs to look wholesaler delivery.
            sem_post(&(sems.ingr_ready)); 
        }
        sem_post(&(sems.lock_ingr));
    }

    return NULL;
}

void 
detect_absence_ings(int inf_ings, char** ingr1, char** ingr2){
    
    int index=0;
    char* arr[2];

    if((inf_ings & 1)==0)
        arr[index++]="milk";

    if((inf_ings & 2)==0)
        arr[index++]="flour";

    if((inf_ings & 4)==0 && index<2)
        arr[index++]="walnuts";

    if((inf_ings & 8)==0 && index<2)
        arr[index++] ="sugar";

    *ingr1 = arr[0];
    *ingr2 = arr[1];
}

int 
wholesaler_func(){

    char line[3];
    char *wait_msg = "The wholesaler is waiting for the dessert\n";
    char *obtain_msg = "the wholesaler has obtained the dessert and left to sell it\n";

    while(read(fd, line, 3)==3){
        *new_ings = 0;
        for(int i=0; i<2; ++i){
            switch (line[i])
            {
                case 'M':
                    *new_ings += 1;
                    break;
                case 'F':
                    *new_ings += 2;
                    break;
                case 'W':
                    *new_ings += 4;
                    break;
                case 'S':
                    *new_ings += 8;
                    break;
            }
        }

        sem_post(&(sems.ingr_ready));
        write(STDERR_FILENO, wait_msg, strlen(wait_msg));
        sem_wait(&(sems.dess_ready));
        write(STDERR_FILENO, obtain_msg, strlen(obtain_msg));

    }
  
    // notify chef threads to be finished
    *new_ings = TERMINATE_CHEF;
    sem_post(&(sems.ingr_ready));

    return 0;
}

void
init_sems(Sems* sems){

    sem_init(&(sems->dess_ready), 0, 0);
    sem_init(&(sems->ingr_ready), 0, 0);
    sem_init(&(sems->lock_ingr), 0, 1);
}

void
destroy_sems(Sems* sems){

    sem_destroy(&(sems->dess_ready));
    sem_destroy(&(sems->ingr_ready));
    sem_destroy(&(sems->lock_ingr));
}

void
print_usage(char *process_name){

    char *template = "Usage : %s -i filePath\n";
    char message[strlen(template)+PROCESS_NAME_LEN];

    snprintf(message, strlen(template)+PROCESS_NAME_LEN, template, process_name);

    write(STDERR_FILENO, message, strlen(message));
}

void
err_exit(char* error_msg){

    write(STDERR_FILENO, error_msg, strlen(error_msg));
    exit(EXIT_FAILURE);
}

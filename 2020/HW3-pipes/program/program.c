#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

//***** USER DEFINED LIBS
#include "piping.h"
#include "base.h"
#include "calc.h"
#include "io_helper.h"
/////////////////////////// GLOBAL VARIABLES ///////////////////////////////////
int fd_a, fd_b;

/////////////////////////// FUNCTION PROTOTYPES ////////////////////////////////
void clean_resources(); // at exit function
int do_homework(int fd_a, int fd_b, int n);
void clean_campground(int **matrix_a, int **matrix_b, int matrix_size);
int start_multi_process(int **matrix_a, int **matrix_b, int size, char ** err_msg);
int create_pipes(int pipes[][PIPE_ARR_SIZE], int pipes_num);


////////////////////////// IMPLEMENTATION BEGINS ///////////////////////////////
int
main(int argc, char *argv[]){

    int n;

    atexit(clean_resources);

    if(argc != 7){
        print_usage(argv[0]);
    }

    int option;
    char * temp;
    while((option = getopt(argc, argv, "i:j:n:")) != -1){
        switch(option){
            case 'i':
                fd_a = open_file(INPUT, optarg);
                break;
            case 'j':
                fd_b = open_file(INPUT, optarg);
                break;
            case 'n':
                n = strtol(optarg, &temp, 10);
                break;
            default:
                print_usage(argv[0]);
        }
    }

    char *err_msg;
    if(fd_a == -1 || fd_b == -1){
        err_msg = "One or more file could not be opened successfully.\n";
        write(STDERR_FILENO, err_msg, strlen(err_msg));
        close(fd_a);
        close(fd_b);
        exit(EXIT_FAILURE);
    }
    else if (*temp != '\0'){
        err_msg = "The value of N could not be converted to a long int\n";
        write(STDERR_FILENO, err_msg, strlen(err_msg));
        close(fd_a);
        close(fd_b);
        exit(EXIT_FAILURE);
    }

    do_homework(fd_a, fd_b, n);
    return 0;
}

int
do_homework(int fd_a, int fd_b, int n){

    int **matrix_a, **matrix_b;
    char *err_msg;
    int edge_size = pow(2,n);

    matrix_a = allocate_2D_int_array(edge_size, edge_size);
    matrix_b = allocate_2D_int_array(edge_size, edge_size);

    int read_err_code = 0;

    read_err_code += read_square_matrix(fd_a, matrix_a, edge_size);
    read_err_code += read_square_matrix(fd_b, matrix_b, edge_size);

    if(read_err_code != 0){
        err_msg = "A problem occured during the reading process.\nExiting...\n";
        clean_campground(matrix_a, matrix_b, edge_size);
        write(STDERR_FILENO, err_msg, strlen(err_msg));
        exit(EXIT_FAILURE);
    }

    if(start_multi_process(matrix_a, matrix_b, edge_size, &err_msg) != 0){
        clean_campground(matrix_a, matrix_b, edge_size);
        write(STDERR_FILENO, err_msg, strlen(err_msg));
        exit(EXIT_FAILURE);
    }

    free_2D_int_array(matrix_a, edge_size);
    free_2D_int_array(matrix_b, edge_size);
    return 0;
}

int
start_multi_process(int **matrix_a, int **matrix_b, int edge_size, char **err_msg){

    int pipes[PIPES_NUM][PIPE_ARR_SIZE];
    if(create_pipes(pipes, PIPES_NUM) != 0){
        *err_msg = "An error occured during the pipe creation...\n";
        return -1;
    }

    if(organize_children_processes(pipes, matrix_a, matrix_b, edge_size) == -1){
        clean_campground(matrix_a, matrix_b, edge_size);
        exit(EXIT_FAILURE);
    }
    return 0;
}

void
clean_campground(int **matrix_a, int **matrix_b, int matrix_size){

    free_2D_int_array(matrix_a, matrix_size);
    free_2D_int_array(matrix_b, matrix_size);
}

int
create_pipes(int pipes[][PIPE_ARR_SIZE], int pipes_num){

    int err_val =0;
    for(; pipes_num > 0; --pipes_num){
        err_val += pipe(pipes[pipes_num-1]);
    }

    return err_val;
}

void
clean_resources(){
    // files are being closed...
    close(fd_a);
    close(fd_b);
}

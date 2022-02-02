#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <sys/wait.h>
//-----------------
#include "piping.h"
#include "base.h"
#include "calc.h"
#include "io_helper.h"

////////// Global Variables /////////////////////////////////
sig_atomic_t children_number = 4; // number of children processes
sig_atomic_t child_exit_status;

void
handler(int signum){
    if(signum == SIGCHLD && children_number>0){
        (--children_number);
        int status;
        wait(&status);
        child_exit_status = status;
    }
    else if(signum == SIGINT){
        exit(1);
    }
}

int
product_matrix(int pipe [PIPE_ARR_SIZE]){

    int **result_matrix, **matrix_a, **matrix_b;
    int size_a_i=0, size_a_j=0, size_b_i=0, size_b_j=0;

    if(read_matrix_from_pipe(pipe[0], &matrix_a, &size_a_i, &size_a_j) == -1)
        return -1;
    if(read_matrix_from_pipe(pipe[0], &matrix_b, &size_b_i, &size_b_j) == -1)
        return -1;

    result_matrix = allocate_2D_int_array(size_a_i, size_b_j);
    calc_matrix_product(matrix_a, size_a_i, size_a_j, matrix_b, size_b_i, size_b_j, result_matrix);

    write_matrix_to_pipe(pipe[1], result_matrix, size_a_i, size_b_j);
    free_2D_int_array(result_matrix, size_a_i);
    return 0;
}

int
organize_children_processes(int pipes[][PIPE_ARR_SIZE], int **matrix_a, int **matrix_b, int edge_size){

    struct sigaction sact;
    memset(&sact, 0, sizeof(sact));
    sact.sa_handler = &handler;
    sigaction(SIGCHLD, &sact, NULL);
    sigaction(SIGINT, &sact, NULL);

    int **vertical_1 = allocate_2D_int_array(edge_size, edge_size/2);
    int **vertical_2 = allocate_2D_int_array(edge_size, edge_size/2);
    int **horizontal_1 = allocate_2D_int_array(edge_size/2, edge_size);
    int **horizontal_2 = allocate_2D_int_array(edge_size/2, edge_size);

    // sub matrices are being prepared to send children processes
    fill_sub_matrix(0, edge_size, 0, edge_size/2, matrix_b, vertical_1);
    fill_sub_matrix(0, edge_size, edge_size/2, edge_size, matrix_b, vertical_2);
    fill_sub_matrix(0, edge_size/2, 0, edge_size, matrix_a, horizontal_1);
    fill_sub_matrix(edge_size/2, edge_size, 0, edge_size, matrix_a, horizontal_2);

    send_data_and_fork(pipes, P2_PIPE_INDEX,
            horizontal_1, edge_size/2, edge_size, vertical_1, edge_size, edge_size/2);
    send_data_and_fork(pipes, P3_PIPE_INDEX,
            horizontal_1, edge_size/2, edge_size, vertical_2, edge_size, edge_size/2);
    send_data_and_fork(pipes, P4_PIPE_INDEX,
            horizontal_2, edge_size/2, edge_size, vertical_1, edge_size, edge_size/2);
    send_data_and_fork(pipes, P5_PIPE_INDEX,
            horizontal_2, edge_size/2, edge_size, vertical_2, edge_size, edge_size/2);

    // wait for processes to response
    int **res_matrix = allocate_2D_int_array(edge_size, edge_size);
    int **res_quarter;

    // get results from P2
    int q_i, q_j;
    read_matrix_from_pipe(pipes[P2_PIPE_INDEX][0], &res_quarter, &q_i, &q_j);
    close(pipes[P2_PIPE_INDEX][0]);
    put_quarter_to_matrix(res_matrix, edge_size, edge_size, res_quarter, P2_PIPE_INDEX);
    free_2D_int_array(res_quarter, edge_size/2);
    // get results from P3
    read_matrix_from_pipe(pipes[P3_PIPE_INDEX][0], &res_quarter, &q_i, &q_j);
    close(pipes[P3_PIPE_INDEX][0]);
    put_quarter_to_matrix(res_matrix, edge_size, edge_size, res_quarter, P3_PIPE_INDEX);
    free_2D_int_array(res_quarter, edge_size/2);
    // get results from P4
    read_matrix_from_pipe(pipes[P4_PIPE_INDEX][0], &res_quarter, &q_i, &q_j);
    close(pipes[P4_PIPE_INDEX][0]);
    put_quarter_to_matrix(res_matrix, edge_size, edge_size, res_quarter, P4_PIPE_INDEX);
    free_2D_int_array(res_quarter, edge_size/2);
    // get results from P5
    read_matrix_from_pipe(pipes[P5_PIPE_INDEX][0], &res_quarter, &q_i, &q_j);
    close(pipes[P5_PIPE_INDEX][0]);
    put_quarter_to_matrix(res_matrix, edge_size, edge_size, res_quarter, P5_PIPE_INDEX);
    free_2D_int_array(res_quarter, edge_size/2);

    print_matrix(res_matrix, edge_size, edge_size);

    free_2D_int_array(res_matrix, edge_size);
    free_2D_int_array(vertical_1, edge_size);
    free_2D_int_array(vertical_2, edge_size);
    free_2D_int_array(horizontal_1, edge_size/2);
    free_2D_int_array(horizontal_2, edge_size/2);

    return 0;
}

int
send_data_and_fork(int pipes[][PIPE_ARR_SIZE], int pipe_index, int **first_matrix,
        int f_size_i,int f_size_j, int **second_matrix, int s_size_i, int s_size_j){

    write_matrix_to_pipe(pipes[pipe_index][1], first_matrix, f_size_i, f_size_j);
    write_matrix_to_pipe(pipes[pipe_index][1], second_matrix, s_size_i, s_size_j);

    int pid = fork();
    switch (pid) {
        case -1:
            return -1;
        case 0:
            close_unused_pipes(pipes, PIPES_NUM, pipe_index);
            product_matrix(pipes[pipe_index]);
            close(pipes[pipe_index][0]);
            close(pipes[pipe_index][1]);
            kill(getppid(), SIGCHLD);
            exit(0);
        default:
            close(pipes[pipe_index][1]);
            return pid;

    }
}

void
put_quarter_to_matrix(int**res_matrix, int size_i, int size_j, int **res_quarter, int quarter_index){

    if(quarter_index == P2_PIPE_INDEX){
        for(int i=0; i<size_i/2; ++i){
            for(int j=0; j<size_j/2; ++j){
                res_matrix[i][j] = res_quarter[i][j];
            }
        }
    }
    else if(quarter_index == P3_PIPE_INDEX){
        for(int i=0; i<size_i/2; ++i){
            for(int j=0; j<size_j/2; ++j){
                res_matrix[i][(size_j/2)+j] = res_quarter[i][j];
            }
        }
    }
    else if(quarter_index == P4_PIPE_INDEX){
        for(int i=0; i<size_i/2; ++i){
            for(int j=0; j<size_j/2; ++j){
                res_matrix[(size_i/2)+i][j] = res_quarter[i][j];
            }
        }
    }
    else if(quarter_index == P5_PIPE_INDEX){
        for(int i=0; i<size_i/2; ++i){
            for(int j=0; j<size_j/2; ++j){
                res_matrix[(size_i/2)+i][(size_j/2)+j] = res_quarter[i][j];
            }
        }
    }

}

void
fill_sub_matrix(int start_i, int end_i, int start_j, int end_j, int ** source, int ** target){

    for(int i=start_i; i<end_i; ++i){
        for(int j=start_j; j<end_j; ++j){
            target[i-start_i][j-start_j] = source[i][j];
        }
    }
}

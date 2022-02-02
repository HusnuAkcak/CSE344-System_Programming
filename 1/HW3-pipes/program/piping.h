#ifndef _PIPING_H_
#define _PIPING_H_

#include "base.h"
#include <signal.h>


////////// Function Prototypes /////////////////////////////////
void handler(int signum);
int send_data_and_fork(int pipes[][PIPE_ARR_SIZE], int pipe_index, int **first_matrix,
        int f_size_i,int f_size_j, int **second_matrix, int s_size_i, int s_size_j);
int product_matrix(int pipe[PIPE_ARR_SIZE]);
int organize_children_processes(int pipes[][PIPE_ARR_SIZE], int **matrix_a, int **matrix_b, int size);
void fill_sub_matrix(int start_i, int end_i, int start_j, int end_j, int ** source, int ** target);
void put_quarter_to_matrix(int**res, int size_i, int size_j, int **res_quarter, int quarter_index);

#endif

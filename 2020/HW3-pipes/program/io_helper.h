#ifndef _IO_HELPER_H_
#define _IO_HELPER_H_

#include "base.h"

#define PROCESS_NAME_LEN 25

// promt operations
void print_usage(char *process_name);

// file operations
int open_file(f_type type, char * file_path);
int read_square_matrix(int fd, int **matrix, int n);

// debug functions
void print_matrix(int **matrix, int size_i, int size_j);

// pipe io
int write_matrix_to_pipe(int fd, int **matrix, int size_i, int size_j);
int read_matrix_from_pipe(int fd, int ***matrix, int *size_i, int *size_j);
void close_unused_pipes(int pipes[][PIPE_ARR_SIZE], int pipes_num, int used_index);

#endif

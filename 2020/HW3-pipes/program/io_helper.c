#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
//------------------
#include "io_helper.h"
#include "base.h"

void
print_usage(char *process_name){

    char *template = "Usage : %s -i inputPathA -j inputPathB -n 8 \n";
    char message[strlen(template)+PROCESS_NAME_LEN];

    snprintf(message, strlen(template)+PROCESS_NAME_LEN, template, process_name);

    write(STDERR_FILENO, message, strlen(message));
}

int
open_file(f_type type, char * file_path){

    int fd;
    mode_t in_mode, out_mode;

    in_mode = S_IRUSR | S_IRGRP | S_IROTH;
    out_mode = S_IWUSR | S_IWGRP | S_IWOTH;

    switch(type){
        case INPUT:
            fd = open(file_path, O_RDONLY, in_mode);
            break;
        case OUTPUT:
            fd = open(file_path, O_WRONLY, out_mode);
            break;
        default:
            return -1; // indicates error
    }

    return fd;
}

//returns 0 on success, returns 1 on failure
int
read_square_matrix(int fd, int **matrix, int size){

    char buff[size+1];
    int i, j;
    for(i=0; i<size; ++i){
        // if bytes are not equal to the size + 1 (new line char)
        if(read(fd, buff, size+1) != size+1)
            return 1;

        for(j=0; j<size; ++j){
            matrix[i][j] = buff[j];
        }
    }

    return 0;
}

int
read_matrix_from_pipe(int fd, int ***matrix, int *m_size_i, int *m_size_j){

    int size_i, size_j;

    if(read(fd, &size_i, sizeof(size_i)) == -1)
        return -1;
    *m_size_i = size_i;

    if(read(fd, &size_j, sizeof(size_j)) == -1)
        return -1;
    *m_size_j = size_j;

    *matrix = allocate_2D_int_array(size_i, size_j);

   for(int i=0; i<size_i; ++i){
       for(int j=0; j<size_j; ++j){
           if(read(fd, &((*matrix)[i][j]), sizeof((*matrix)[i][j])) == -1){
               return -1;
           }
       }
   }
   return 0;
}

int
write_matrix_to_pipe(int fd, int **matrix, int size_i, int size_j){

    if(write(fd, &size_i, sizeof(size_i)) == -1)
        return -1;

    if(write(fd, &size_j, sizeof(size_j)) == -1)
        return -1;

    for(int i=0; i<size_i; ++i){
        for(int j=0; j<size_j; ++j){
            if(write(fd, &matrix[i][j], sizeof(matrix[i][j])) == -1)
                return -1;
        }
    }
    return 0;
}

void
close_unused_pipes(int pipes[][PIPE_ARR_SIZE], int pipes_num, int used_index){

    for(int i=0; i< pipes_num; ++i){
        if(i!=used_index){
            close(pipes[i][0]);
            close(pipes[i][1]);
        }
    }
}

void
print_matrix(int **matrix, int size_i, int size_j){

    char buff[11];
    for(int i=0; i< size_i; ++i){
        for(int j=0; j<size_j; ++j){
            snprintf(buff, sizeof(buff), "%d %c", matrix[i][j], 0);
            write(STDERR_FILENO, buff, strlen(buff));
        }
        buff[0] = '\n';
        write(STDERR_FILENO, buff, 1);
    }
}

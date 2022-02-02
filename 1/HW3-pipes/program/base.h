#ifndef _BASE_H_
#define _BASE_H_

typedef enum {INPUT = 'i', OUTPUT = 'o', IN_OUT = 'b'}f_type;

//***** MACROS
#define PIPES_NUM 4         // number of pipes that will be used
#define PIPE_ARR_SIZE 2     // size of the array which keeps pipe fd's

// pipe index of children processes
// P2 PIPES
#define P2_PIPE_INDEX 0
// P3 PIPES
#define P3_PIPE_INDEX 1
// P4 PIPES
#define P4_PIPE_INDEX 2
// P5 PIPES
#define P5_PIPE_INDEX 3

//***** END OF MACROS

typedef struct {
    int **matrix_portion;
    int pos_i, pos_j, size_i, size_j;
}quarter;


int ** allocate_2D_int_array(int r, int c);
void free_2D_int_array(int** matrix, int n);
#endif

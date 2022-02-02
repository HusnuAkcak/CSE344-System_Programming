#include <stdlib.h>
#include "base.h"

int **
allocate_2D_int_array(int row, int col){
    int **matrix;

    matrix = (int**)(malloc(row * sizeof(int*)));
    for(int i=0; i < row; ++i)
        matrix[i] = (int*)(malloc(col * sizeof(int)));

    return matrix;
}

void
free_2D_int_array(int** matrix, int row){

    for(int i=0; i<row; ++i)
        free(matrix[i]);

    free(matrix);
}

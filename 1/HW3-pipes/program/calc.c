#include "calc.h"
#include <stdio.h>

int
calc_matrix_product(int **matrix_a, int a_i, int a_j, int **matrix_b, int b_i, int b_j, int **matrix_res){

    for(int i=0; i< a_i; ++i){
        for(int j=0; j< b_j; ++j){
            matrix_res[i][j] = 0;
            for(int k=0; k< a_j; ++k){
                matrix_res[i][j] += matrix_a[i][k]*matrix_b[k][j];
            }
        }
    }
    return 0;
}

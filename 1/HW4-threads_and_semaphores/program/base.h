#ifndef _BASE_H_
#define _BASE_H_

#include <semaphore.h>

///////////////// UDT's ////////////////////////////
typedef enum {M=1, F=2, W=4, S=8} ingredients;

///////////////// STRUCTS //////////////////////////
typedef struct{
    sem_t dess_ready; // dessert ready for wholesaler to take
    sem_t ingr_ready; // ingredients ready for chefs to take
    sem_t lock_ingr;  // protects new_ings 
}Sems;

typedef struct {

    int chf_index;
    int chf_ings; // chefs infinite ingredientss
}thArg;

#endif

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <semaphore.h>
#include "base.h"
#include "cook.h"

void
work_as_cook(shMem* mem, int index){

    char* waitTemp = "Cook %d is going to the kitchen to wait for/get a plate --- kitchen items P:%d, C:%d, D:%d = %d\n";
    char* servTemp = "Cook %d is going to counter to deliver %s ----------------- counter items P:%d, C:%d, D:%d = %d\n";
    char* afterServTemp = "Cook %d placed %s on the counter --------------------- counter items P:%d, C:%d, D:%d = %d\n";
    char* goodByTemp = "Cook %d has finished serving ---------------------------- items at kitchen : %d – going home – GOODBYE!!!\n";

    int msgLen = strlen(waitTemp)+70;
    char msg[msgLen];
    int platesInKitchen;
    bool cont;

    sem_wait(&(mem->mCook));
    empty_tray(mem); // make empty curr tray first
    sem_post(&(mem->mCook));

    ////fprintf(stderr, "before wait mPlate 4\n");
    sem_wait(&(mem->mPlate));
    platesInKitchen = (mem->p)+(mem->c)+(mem->d);
    sem_post(&(mem->mPlate));

    sem_wait(&(mem->mSupply));
    cont= mem->supply;
    sem_post(&(mem->mSupply));

    while(platesInKitchen >0 || cont == true){

       //fprintf(stderr, "before wait mPlate 1\n");
        sem_wait(&(mem->mPlate));
        snprintf(msg, msgLen, waitTemp, index, (mem->p), (mem->c), (mem->d), ((mem->p)+(mem->c)+(mem->d)));
        write(STDERR_FILENO, msg, strlen(msg));
        sem_post(&(mem->mPlate));

        // sem_wait(&(mem->mTryCook));
        sem_wait(&(mem->mCook));
        if((mem->currT).p == 0){
            (mem->currT).p =1;
            sem_post(&(mem->mCook));

            sem_wait(&(mem->mSoup));
            if(mem->onSoup == false){
               //fprintf(stderr, "]]]]before wait soup mPlate 2\n");
                sem_wait(&(mem->kSoup));
               //fprintf(stderr, "]]]]after wait soup mPlate 2\n");

                sem_post(&(mem->kTotalPlate));
                sem_wait(&(mem->mPlate));
                --(mem->p);
                sem_post(&(mem->mPlate));

                ////fprintf(stderr, "before waiting mTray 1 Cook %d\n", index);
                sem_wait(&(mem->mTray));
                snprintf(msg, msgLen, servTemp, index, "soup", (mem->trayCounts), (mem->trayCounts), (mem->trayCounts), 3*(mem->trayCounts));
                write(STDERR_FILENO, msg, strlen(msg));
                sem_post(&(mem->mTray));

                // sem_wait(&(mem->counterRooms));

                sem_wait(&(mem->mTray));
                snprintf(msg, msgLen, afterServTemp, index, "soup", (mem->trayCounts)+1, (mem->trayCounts), (mem->trayCounts), 3*(mem->trayCounts)+1);
                write(STDERR_FILENO, msg, strlen(msg));
                sem_post(&(mem->mTray));

                mem->onSoup = true;
            }
            sem_post(&(mem->mSoup));
        }
        else if((mem->currT).c == 0){
            (mem->currT).c =1;
            sem_post(&(mem->mCook));

            sem_wait(&(mem->mMainC));
            if(mem->onMainC == false){
                ////fprintf(stderr, "before wait mPlate 2\n");
                sem_wait(&(mem->kMainCourse));
                sem_post(&(mem->kTotalPlate));
                sem_wait(&(mem->mPlate));
                --(mem->c);
                sem_post(&(mem->mPlate));

                ////fprintf(stderr, "before waiting mTray 2 Cook %d\n", index);
                sem_wait(&(mem->mTray));
                snprintf(msg, msgLen, servTemp, index, "main course", (mem->trayCounts)+1, (mem->trayCounts), (mem->trayCounts), 3*(mem->trayCounts)+1);
                write(STDERR_FILENO, msg, strlen(msg));
                sem_post(&(mem->mTray));

                int val;
                sem_getvalue(&(mem->counterRooms), &val);
               //fprintf(stderr, "*************************counterRooms val %d\n", val);
                // sem_wait(&(mem->counterRooms));

                sem_wait(&(mem->mTray));
                snprintf(msg, msgLen, afterServTemp, index, "main course", (mem->trayCounts)+1, (mem->trayCounts)+1, (mem->trayCounts), 3*(mem->trayCounts)+2);
                write(STDERR_FILENO, msg, strlen(msg));
                sem_post(&(mem->mTray));

                mem->onMainC = true;
            }
            sem_post(&(mem->mMainC));

        }
        else if((mem->currT).d == 0){
            (mem->currT).d =1;
            sem_post(&(mem->mCook));

            sem_wait(&(mem->mDessert));
            if(mem->onDessert == false){
                ////fprintf(stderr, "before wait mPlate 3\n");
                sem_wait(&(mem->kDessert));
                sem_post(&(mem->kTotalPlate));
                sem_wait(&(mem->mPlate));
                --(mem->d);
                sem_post(&(mem->mPlate));

                ////fprintf(stderr, "before waiting mTray 3 Cook %d\n", index);
                sem_wait(&(mem->mTray));
                snprintf(msg, msgLen, servTemp, index, "dessert", (mem->trayCounts)+1, (mem->trayCounts)+1, (mem->trayCounts), 3*(mem->trayCounts)+2);
                write(STDERR_FILENO, msg, strlen(msg));
                sem_post(&(mem->mTray));

                // sem_wait(&(mem->counterRooms));

                sem_wait(&(mem->mTray));
                snprintf(msg, msgLen, afterServTemp, index, "dessert", (mem->trayCounts)+1, (mem->trayCounts)+1, (mem->trayCounts)+1, 3*(mem->trayCounts)+3);
                write(STDERR_FILENO, msg, strlen(msg));
                sem_post(&(mem->mTray));

                mem->onDessert = true;
            }
            sem_post(&(mem->mDessert));
        }
        else{
            empty_tray(mem);
            sem_post(&(mem->mCook));

            sem_wait(&(mem->counterRooms));
            sem_post(&(mem->trays)); // when curr tray complated it's ready to serve

            sem_wait(&(mem->mTray));
            // sem_wait(&(mem->counterRooms));
            // sem_wait(&(mem->counterRooms));
            ++(mem->trayCounts);
            sem_post(&(mem->mTray));


            ////fprintf(stderr, "???????????In reset block. Cook %d\n", index);
        }

        // sem_post(&(mem->mTryCook));

        ////fprintf(stderr, "before wait mPlate 4\n");
        sem_wait(&(mem->mPlate));
        platesInKitchen = (mem->p)+(mem->c)+(mem->d);
        sem_post(&(mem->mPlate));

        sem_wait(&(mem->mSupply));
        cont= mem->supply;
        sem_post(&(mem->mSupply));
    }

    snprintf(msg, msgLen, goodByTemp, index, platesInKitchen);
    write(STDERR_FILENO, msg, strlen(msg));
}

void
empty_tray(shMem* mem){

   //fprintf(stderr, "before mSoup\n");
    sem_wait(&(mem->mSoup));
   //fprintf(stderr, "before mMainC\n");
    sem_wait(&(mem->mMainC));
   //fprintf(stderr, "before mDessert\n");
    sem_wait(&(mem->mDessert));
    mem->onSoup = false;
    mem->onMainC = false;
    mem->onDessert = false;

    sem_post(&(mem->mDessert));
    sem_post(&(mem->mMainC));
    sem_post(&(mem->mSoup));


    mem->currT.p=0;
    mem->currT.c=0;
    mem->currT.d=0;

}

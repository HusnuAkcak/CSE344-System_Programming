#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <semaphore.h>
#include "student.h"
#include "base.h"

void
eat_as_und(shMem* mem, int index){

    int remainRound = L;

    char* waitTemp = "Student %d is going to the counter (round %d) \n\t"
                     "---> # of students at counter: %d and counter items P:%d, C:%d, D:%d = %d\n";
    char* waitTableTemp = "Student %d got food and is going to get a table (round %d) - # of empty tables: %d\n";
    char* sitEatTemp = "Student %d sat at table %d to eat (round %d) - empty tables:%d\n";
    char* doneEatTemp = "Student %d left table %d to eat again (round %d) - empty tables:%d\n";
    char* goodByTemp = "Student %d is done eating L=%d times - going home – GOODBYE!!!\n";

    int msgLen = strlen(waitTemp)+80;
    char msg[msgLen];

    while(remainRound>0){

        sem_wait(&(mem->mCook));
        sem_wait(&(mem->mTray));
        sem_wait(&(mem->mSt));
        snprintf(msg, msgLen, waitTemp, index,
                    (L-remainRound+1),
                    (mem->cUndSt)+(mem->cGradSt),
                    (mem->trayCounts)+(mem->currT).p, (mem->trayCounts)+(mem->currT).c, (mem->trayCounts)+(mem->currT).d,
                    ((mem->trayCounts)+((mem->currT).p)+((mem->currT).c)+((mem->currT).d)));
        write(STDERR_FILENO, msg, strlen(msg));
        sem_post(&(mem->mSt));
        sem_post(&(mem->mTray));
        sem_post(&(mem->mCook));

        // priority operation will be performed.
        //fprintf(stderr, "////////BEFORE  %d\n", index);
        sem_wait(&(mem->mStTry));
        sem_post(&(mem->counterRooms));
        sem_wait(&(mem->trays)); // attemp to take a tray from the counter

        //fprintf(stderr, "////////AFTER  %d\n", index);

        //fprintf(stderr, "before mTray student %d\n", index);
        sem_wait(&(mem->mTray));
        // sem_post(&(mem->counterRooms));
        // sem_post(&(mem->counterRooms));

        --(mem->trayCounts);
        sem_post(&(mem->mTray));
        sem_post(&(mem->mStTry));
        

        sem_wait(&(mem->mTb));
        snprintf(msg, msgLen, waitTableTemp, index, (L-remainRound+1), mem->cFreeTb);
        write(STDERR_FILENO, msg, strlen(msg));
        sem_post(&(mem->mTb));

        sem_wait(&(mem->mSt));
        --(mem->cUndSt);
        sem_post(&(mem->mSt));

        //fprintf(stderr, "]]]] before %d waits for an empyt table\n", index);
        sem_wait(&(mem->fTables));
        //fprintf(stderr, "]]]] after %d waits for an empyt table\n", index);

        sem_wait(&(mem->mTb));
        --(mem->cFreeTb);
        snprintf(msg, msgLen, sitEatTemp, index, (mem->cFreeTb)+1, (L-remainRound+1), mem->cFreeTb);
        write(STDERR_FILENO, msg, strlen(msg));
        sem_post(&(mem->mTb));

        sem_post(&(mem->fTables));

        sem_wait(&(mem->mTb));
        ++(mem->cFreeTb);
        snprintf(msg, msgLen, doneEatTemp, index, (mem->cFreeTb), (L-remainRound+1), mem->cFreeTb);
        write(STDERR_FILENO, msg, strlen(msg));
        sem_post(&(mem->mTb));

        --remainRound;

        sem_wait(&(mem->mSt));
        ++(mem->cUndSt);
        sem_post(&(mem->mSt));

    }

    snprintf(msg, msgLen, goodByTemp, index, L);
    write(STDERR_FILENO, msg, strlen(msg));

}

void
eat_as_grad(shMem* mem, int index){


}

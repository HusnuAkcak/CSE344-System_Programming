#ifndef _BASE_H_
#define _BASE_H_

/////////////////////////// USER DEFINED TYPES  ////////////////////////////////
typedef enum { false, true } bool;
typedef struct { int p, c, d; }tray;
typedef struct{
    sem_t   kSoup,          // soup plates in the kitchen
            kMainCourse,    // mainCourse plates in the kitchen
            kDessert,       // dessert plates in the kitchen
            kTotalPlate,    // total plates in the kitchen
            trays,          // trays on the counter
            counterRooms,   // reamining capacity on the counter as size of plate
            fTables,        // free tables
            tryTakeTray;    // for establishing btw undergraduate and graduate students

    // mutexes
    sem_t   mTb,     // to protect 'cFreeTb'
            mSt,     // to protect 'cUndSt' and 'cGradSt'
            mStTry,  // while taking plates(tray) from counter
            mTray,   // to protect 'trayCounts'
            mCook,   // to protect 'currT'
            mTryCook,// to prevent more than one cook is working on the same code
            mSoup,   // to provide only one soup placement attemp over a tray
            mMainC,  // to provide only one main course placement attemp over a tray
            mDessert, // to provide only one dessert placement attemp over a tray
            mSupply, // to protect 'supply'
            mPlate;  // to protect 'p', 'c', 'd'

    int p, c, d,            // *kitchen* data of soup, maincourse and dessert
        cUndSt, cGradSt,    // counter und
        cFreeTb,            // num of available table
        trayCounts;         // number of trays on the counter.

    bool supply, onSoup, onMainC, onDessert;

    tray currT;             // to make the counter continue, firstly current tray is made full.
}shMem;

///////////////////////////     MACROS          ////////////////////////////////
#define BASE 10

/////////////////////////// GLOBAL VARIABLES    ////////////////////////////////
int N, M, U, G, T, L, S, K;

#endif

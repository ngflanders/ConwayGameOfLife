#ifndef BAND_H
#define BAND_H

#include <semaphore.h>
#include <pthread.h>

typedef struct {
   pthread_mutex_t botMutex;
   pthread_mutex_t topMutex;
   int dependCount;
   int boardNum;
   int bandNum;
   int startRow;
   int width;
} Band;

#endif

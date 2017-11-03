#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include "Queue.h"
#include "SmartAlloc.h"
#include "Band.h"
#include <semaphore.h>
#include <pthread.h>

//clang Conway.c Queue.c SmartAlloc.c -lpthread -o Conway
//./Conway < glidergun.in

static int rowMAX;
static int colMAX;
static int numBands;
static int numBoards;
static int cycles;
static int finishBoard;
static pthread_mutex_t cycMutex;
static pthread_mutex_t printMutex;

typedef struct Cell {
   int lifeState;
   int nCount;
} Cell;

typedef struct Generation {
   Cell **board;
   Band *bands;
   int bandsCompleted;
   pthread_mutex_t finishGenMutex;
} Generation;

typedef struct {
   Queue *queue;
   int id;
} ThreadInfo;

static Generation *gens;

static void InitGen(Generation *gen, int boardNum) {
   int isEdgeBand;
   gen->board = calloc(sizeof(Cell *), rowMAX);

   for (int i = 0; i < rowMAX; i++)
      gen->board[i] = calloc(sizeof(Cell), colMAX);

   gen->bands = malloc(sizeof(Band) * numBands);

   for (int bandNum = 0; bandNum < numBands; bandNum++) {
      isEdgeBand = bandNum == 0 || bandNum == numBands - 1;
      gen->bands[bandNum].bandNum = bandNum;
      gen->bands[bandNum].boardNum = boardNum;
      gen->bands[bandNum].startRow = rowMAX / numBands * bandNum;
      gen->bands[bandNum].width = rowMAX / numBands;
      gen->bands[bandNum].dependCount = isEdgeBand ? 2 : 3;
      pthread_mutex_init(&gen->bands[bandNum].topMutex, NULL);
      pthread_mutex_init(&gen->bands[bandNum].botMutex, NULL);
   }

   gen->bandsCompleted = 0;
   pthread_mutex_init(&gen->finishGenMutex, NULL);
}

static void PrintBoard(Cell **cell) {
   usleep(100000);
   system("clear");
   for (int row = 0; row < rowMAX; row++) {
      for (int col = 0; col < colMAX; col++)
         printf(cell[row][col].lifeState ? "X  " : "_  ");
      printf("\n");
   }
}

static void CopyPrevNeighbors(Cell **prevGen, Cell **nextGen) {
   for (int row = 0; row < rowMAX; row++)
      for (int col = 0; col < colMAX; col++)
         nextGen[row][col].nCount = prevGen[row][col].nCount;
}

static void UpCopyNextBandNeighbors(Band *band) {
   int nextBrdNum = band->boardNum == (numBoards - 1) ? 0 : band->boardNum + 1;
   int rowLimit = band->width + (band->bandNum == numBands - 1 ? 0 : 1);
   int start = band->startRow;
   Cell **curGen = gens[band->boardNum].board;
   Cell **nextGen = gens[nextBrdNum].board;

   for (int row = (start ? -1 : 0); row < rowLimit; row++)
      for (int col = 0; col < colMAX; col++)
         nextGen[row + start][col].nCount = curGen[row + start][col].nCount;
}

void UpdateNeighbors(int boardNum, int row, int col, int incr) {
   Cell **board = gens[boardNum].board;

   if (row > 0)
      board[row - 1][col].nCount += incr;
   if (col > 0)
      board[row][col - 1].nCount += incr;
   if (row < rowMAX - 1)
      board[row + 1][col].nCount += incr;
   if (col < colMAX - 1)
      board[row][col + 1].nCount += incr;
   if (row > 0 && col > 0)
      board[row - 1][col - 1].nCount += incr;
   if (row < rowMAX - 1 && col < colMAX - 1)
      board[row + 1][col + 1].nCount += incr;
   if (row < rowMAX - 1 && col > 0)
      board[row + 1][col - 1].nCount += incr;
   if (row > 0 && col < colMAX - 1)
      board[row - 1][col + 1].nCount += incr;
}

static void FillNextGenBand(Band *band) {
   int neighborCount, lifeState, botRow, topRow, botLock, topLock;
   int myBand = band->bandNum;
   int nextBoardNum = band->boardNum;
   int prevBoardNum = band->boardNum == 0 ? numBoards - 1 : band->boardNum - 1;
   int start = band->startRow;
   Cell **prevGen = gens[prevBoardNum].board;
   Cell **nextGen = gens[nextBoardNum].board;

   for (int row = 0; row < band->width; row++) {
      botRow = row == band->width - 2 || row == band->width - 1;
      topRow = row == 0 || row == 1;

      botLock = botRow && myBand != numBands - 1;
      topLock = topRow && myBand != 0;

      if (botLock) {
         pthread_mutex_lock(&band->botMutex);
         pthread_mutex_lock(&gens[nextBoardNum].bands[myBand + 1].topMutex);
      }
      if (topLock) {
         pthread_mutex_lock(&gens[nextBoardNum].bands[myBand - 1].botMutex);
         pthread_mutex_lock(&band->topMutex);
      }

      for (int col = 0; col < colMAX; col++) {
         neighborCount = prevGen[row + start][col].nCount;
         lifeState = prevGen[row + start][col].lifeState;
         if ((neighborCount < 2 || neighborCount > 3) && lifeState) {
            nextGen[row + start][col].lifeState = 0;
            UpdateNeighbors(nextBoardNum, row + start, col, -1);
         } else if (neighborCount == 3 && !lifeState) {
            nextGen[row + start][col].lifeState = 1;
            UpdateNeighbors(nextBoardNum, row + start, col, 1);
         } else
            nextGen[row + start][col].lifeState = lifeState;
      }
      if (botLock) {
         pthread_mutex_unlock(&band->botMutex);
         pthread_mutex_unlock(&gens[nextBoardNum].bands[myBand + 1].topMutex);
      }
      if (topLock) {
         pthread_mutex_unlock(&gens[nextBoardNum].bands[myBand - 1].botMutex);
         pthread_mutex_unlock(&band->topMutex);
      }
   }
}

static void DecNextGenSem(Band *band, Queue *queue) {
   int nextBrdNum = band->boardNum == (numBoards - 1) ? 0 : band->boardNum + 1;
   int curBandNum = band->bandNum;
   Generation *gen = &gens[nextBrdNum];

   if (curBandNum != 0) {
      pthread_mutex_lock(&gen->bands[curBandNum - 1].topMutex);
      gen->bands[curBandNum - 1].dependCount--;

      if (gen->bands[curBandNum - 1].dependCount == 0) {
         pthread_mutex_lock(&cycMutex);
         if (!(cycles < numBands && gen->bands->boardNum == finishBoard + 1))
            RQAdd(queue, &gen->bands[curBandNum - 1]);
         pthread_mutex_unlock(&cycMutex);
      }
      pthread_mutex_unlock(&gen->bands[curBandNum - 1].topMutex);
   }
// ---------------------------------------------------------------------
   pthread_mutex_lock(&gen->bands[curBandNum].topMutex);
   gen->bands[curBandNum].dependCount--;

   if (gen->bands[curBandNum].dependCount == 0) {
      pthread_mutex_lock(&cycMutex);
      if (!(cycles < numBands && gen->bands->boardNum == finishBoard + 1))
         RQAdd(queue, &gen->bands[curBandNum]);
      pthread_mutex_unlock(&cycMutex);
   }
   pthread_mutex_unlock(&gen->bands[curBandNum].topMutex);

// ---------------------------------------------------------------------
   if (curBandNum != numBands - 1) {
      pthread_mutex_lock(&gen->bands[curBandNum + 1].topMutex);
      gen->bands[curBandNum + 1].dependCount--;

      if (gen->bands[curBandNum + 1].dependCount == 0) {
         pthread_mutex_lock(&cycMutex);
         if (!(cycles < numBands && gen->bands->boardNum == finishBoard + 1))
            RQAdd(queue, &gen->bands[curBandNum + 1]);
         pthread_mutex_unlock(&cycMutex);
      }
      pthread_mutex_unlock(&gen->bands[curBandNum + 1].topMutex);
   }
}

static void PlaceLifeForm(int row, int col) {
   gens[0].board[row][col].lifeState = 1;
   UpdateNeighbors(0, row, col, 1);
}

static void PrimeQueue(Queue *queue, Band *band) {
   for (int i = 0; i < numBands; i++)
      RQAdd(queue, &band[i]);
}

static void WipeGen(Generation *gen) {
   int bandNum = numBands;
   int isEdgeBand;

   while (bandNum--) {
      isEdgeBand = bandNum == 0 || bandNum == numBands - 1;
      gen->bands[bandNum].dependCount = isEdgeBand ? 2 : 3;
   }
   gen->bandsCompleted = 0;
}

void *ThreadMain(void *vInfo) {
   ThreadInfo *info = vInfo;

   while (1) {
      pthread_mutex_lock(&cycMutex);
      if (!cycles) {
         pthread_mutex_unlock(&cycMutex);
         return NULL;
      } else {
         cycles--;
         pthread_mutex_unlock(&cycMutex);
      }

      Band *band = RQRemove(info->queue);
      Generation *nextGen = &gens[band->boardNum];

      FillNextGenBand(band);
      UpCopyNextBandNeighbors(band);

      pthread_mutex_lock(&nextGen->finishGenMutex);
      nextGen->bandsCompleted++;
      if (nextGen->bandsCompleted == numBands) {
         pthread_mutex_lock(&printMutex);
         PrintBoard(nextGen->board);
         pthread_mutex_unlock(&printMutex);
         WipeGen(nextGen);
      }
      pthread_mutex_unlock(&nextGen->finishGenMutex);

      DecNextGenSem(band, info->queue);
   }

   return NULL;
}

int main() {
   int tCount, numGens, row, col, thread;
   pthread_t *threads;
   ThreadInfo *info;
   Queue *queue = RQCreate();

   pthread_mutex_init(&cycMutex, NULL);
   pthread_mutex_init(&printMutex, NULL);

   scanf("%d\n%d\n%d\n%d %d", &tCount, &numBands, &numGens, &rowMAX, &colMAX);

   cycles = numGens * numBands;
   threads = malloc(tCount * sizeof(pthread_t));
   numBoards = numBands + 1;
   finishBoard = numGens % numBoards;
   gens = malloc(sizeof(Generation) * numBoards);

   for (int i = 0; i < numBoards; i++)
      InitGen(&gens[i], i);

   while (EOF != scanf("%d %d", &row, &col))
      PlaceLifeForm(row, col);

   PrintBoard(gens[0].board);

   PrimeQueue(queue, gens[1].bands);
   CopyPrevNeighbors(gens[0].board, gens[1].board);

   for (thread = 0; thread < tCount; thread++) {
      info = malloc(sizeof(ThreadInfo));
      info->id = thread;
      info->queue = queue;
      pthread_create(threads + thread, NULL, ThreadMain, info);
   }

   for (thread = 0; thread < tCount; thread++)
      pthread_join(threads[thread], NULL);

   return 0;
}
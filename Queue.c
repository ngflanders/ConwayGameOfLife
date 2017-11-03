#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>
#include "Queue.h"

typedef struct Node {
   Band *band;
   struct Node *next;
} Node;

struct Queue {
   pthread_mutex_t modMutex;
   sem_t qSize;
   Node *head;
   Node *tail;
};

Queue *RQCreate() {
   Queue *rtn = calloc(1, sizeof(struct Queue));

   pthread_mutex_init(&rtn->modMutex, NULL);
   sem_init(&rtn->qSize, 0, 0);

   return rtn;
}

void RQAdd(Queue *q, Band *band) {
   pthread_mutex_lock(&q->modMutex);

   if (!q->head)
      q->tail = q->head = calloc(1, sizeof(Node));
   else {
      q->tail->next = calloc(1, sizeof(Node));
      q->tail = q->tail->next;
   }
   q->tail->next = 0;
   q->tail->band = band;

   sem_post(&q->qSize);
   pthread_mutex_unlock(&q->modMutex);
}

Band *RQRemove(Queue *q) {
   Band *rtn;
   Node *temp;

   sem_wait(&q->qSize);
   pthread_mutex_lock(&q->modMutex);

   while (q->head == NULL)
      ;
   rtn = q->head->band;
   temp = q->head;
   q->head = q->head->next;
   free(temp);

   pthread_mutex_unlock(&q->modMutex);

   return rtn;
}

int RQIsEmpty(Queue *q) {
   return q->head == NULL;
}

void RQDestroy(Queue *q) {
   Node *temp;

   pthread_mutex_lock(&q->modMutex);

   while (q->head) {
      temp = q->head;
      q->head = q->head->next;
      free(temp);
   }
   pthread_mutex_unlock(&q->modMutex);
   free(q);
}


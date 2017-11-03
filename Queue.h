#ifndef QUEUE_H
#define QUEUE_H

#include "Band.h"

typedef struct Queue Queue;

// Create and return a fully-reentrant, empty, Queue
Queue *RQCreate();

// Destroy a Queue, freeing all associated storage
void RQDestroy(Queue *);

// Add a Report to the end of a Queue, reentrantly.
// An arbitrary number of threads may call RQAdd simultaneously,
// and appropriate use of mutex/semaphore will ensure no race conditions.
void RQAdd(Queue *, Band *);

// Report empty state of queue.  (Note this is highly transient)
int RQIsEmpty(Queue *);

// Remove and return one Report from a Queue, reentrantly.  And,
// importantly, if the Queue is empty, block until some Report
// becomes available to return, *without spin waiting*.
Band *RQRemove(Queue *);

#endif

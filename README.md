# ConwayGameOfLife
An multi-threaded implementation of Conway's Game of Life, written in C.

Conway's Game of Life is not really a game, but a simulation. Given some initial condition, one can watch how the environment changes.

The simulation exists within a grid of cells which are either alive or dead. Each cycle of the simulation these cells can change value.

The cells live, die, or are born based on the status of their immediate 8 neighbors.
+ If there are too few neighbors(<2): the cell dies.
+ If there are too many neighbors (>3): the cell dies.
+ If a cell is empty/dead currently, but it has 3 alive neighbors, it becomes alive.
+ A cell continues to live while it has 2 or 3 neighbors.


`clang Conway.c Queue.c SmartAlloc.c -lpthread -o Conway`
`./Conway < glidergun.in`

## Multithreading
Picturing each `Generation` or cycle of the simulation as a new grid to be calculated, we split the grid into `Bands`. These `Bands` which need to be calculated are placed into a `Queue`, from which the various threads will take the next in line and calculate the new values of the cells. With careful use of Mutexes and Semaphores, the threads can properly calculate the new values, and know when a future `Generation`'s `Band` can be placed into the Queue.

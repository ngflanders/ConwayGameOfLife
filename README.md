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

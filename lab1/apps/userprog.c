#include "usertraps.h"

void main (int x)
{
  Printf("Hello World!\n");
  // int pid = 0;
  // pid = Getpid();
  Printf("My process ID is: %d\n", Getpid());
  while(1); // Use CTRL-C to exit the simulator
}

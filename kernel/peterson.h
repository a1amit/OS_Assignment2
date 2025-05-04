// filepath: kernel/peterson.h
//task 1
#ifndef _PETERSON_H_
#define _PETERSON_H_

#include "spinlock.h"

#define NPETERSONLOCKS 16 // Define the number of locks (at least 15)

struct peterson_lock {
  struct spinlock lk;     // Spinlock protecting this structure's fields
  char *name;             // Name of lock (for debugging)
  int active;             // Is this lock slot currently allocated? aka state
  volatile int flag[2];   // Interest flags for role 0 and 1
  volatile int turn;      // Whose turn it is
};

#endif // _PETERSON_H_
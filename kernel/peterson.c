// filepath: kernel/peterson.c
//task 1
#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"     // Include this earlier - it brings in spinlock.h
#include "peterson.h" // Include the new header
#include "proc.h"     // Now include proc.h

// Array to hold all Peterson locks
struct peterson_lock ptable[NPETERSONLOCKS];
// Spinlock to protect allocation/deallocation from ptable
struct spinlock ptable_lock;

// Initialize the Peterson lock table
void
peterson_init(void)
{
  initlock(&ptable_lock, "ptable");
  for(int i = 0; i < NPETERSONLOCKS; i++) {
    // Initialize the spinlock within each Peterson lock structure
    initlock(&ptable[i].lk, "peterson");
    ptable[i].name = "unnamed_peterson"; // Simple default name
    ptable[i].active = 0; // Mark as inactive initially
    ptable[i].flag[0] = 0;
    ptable[i].flag[1] = 0;
    ptable[i].turn = 0;
  }
  // Optional: Add a print statement to confirm initialization
  // printf("peterson_init complete\n");
}

// Allocate a peterson lock from the ptable.
// Returns lock id (index) or -1 if no locks are available.
// Must be called holding ptable_lock.
static int
peterson_alloc(void)
{
  for(int i = 0; i < NPETERSONLOCKS; i++) {
    acquire(&ptable[i].lk); // Lock the individual entry
    if(!ptable[i].active) {
      // Found an inactive lock, allocate it
      ptable[i].active = 1;
      ptable[i].flag[0] = 0;
      ptable[i].flag[1] = 0;
      ptable[i].turn = 0; // Initialize turn (e.g., role 0)
      ptable[i].name = "peterson_lock"; // Assign a name
      release(&ptable[i].lk);
      return i; // Return the index as the lock ID
    }
    release(&ptable[i].lk); // Release if already active
  }
  return -1; // No free locks found
}

// Free a peterson lock (mark as inactive).
// Must be called holding ptable_lock and the lock's internal spinlock ptable[id].lk.
static void
peterson_free(int id)
{
  // Basic validation (should already hold locks, but good practice)
  if(id < 0 || id >= NPETERSONLOCKS || !ptable[id].active)
    panic("peterson_free: invalid id or inactive lock");

  // Reset the lock state
  ptable[id].active = 0;
  ptable[id].flag[0] = 0;
  ptable[id].flag[1] = 0;
  ptable[id].turn = 0;
  ptable[id].name = "unnamed_peterson";
}

// System call: Create a new Peterson lock
uint64
sys_peterson_create(void)
{
  int id;

  acquire(&ptable_lock); // Lock the global table
  id = peterson_alloc(); // Attempt to allocate a lock
  release(&ptable_lock); // Unlock the global table

  return id; // Return the lock ID (-1 if allocation failed)
}

// System call: Acquire a Peterson lock
uint64
sys_peterson_acquire(void)
{
  int lock_id, role;
  struct peterson_lock *pl;

  // Get arguments from user space
  // Fetch arguments first
  argint(0, &lock_id);
  argint(1, &role);

  // Validate arguments *after* fetching
  if(lock_id < 0 || lock_id >= NPETERSONLOCKS || (role != 0 && role != 1)) {
    return -1;
  }

  pl = &ptable[lock_id]; // Get pointer to the specific lock

  acquire(&pl->lk); // Acquire the lock's internal spinlock

  // Check if the lock is valid (active)
  if (!pl->active) {
    release(&pl->lk);
    return -1;
  }

  // Peterson Algorithm: Indicate interest
  pl->flag[role] = 1;
  __sync_synchronize(); // Memory barrier after writing flag

  // Peterson Algorithm: Set turn to the other role
  pl->turn = 1 - role;
  __sync_synchronize(); // Memory barrier after writing turn

  // Peterson Algorithm: Wait condition
  // Loop while the other process is interested AND it's the other process's turn
  while (pl->flag[1 - role] == 1 && pl->turn == (1 - role)) {
      // Before yielding, check if the lock was destroyed concurrently
      if (!pl->active) {
          release(&pl->lk);
          return -1; // Lock destroyed while waiting
      }
      // Release the spinlock before yielding CPU
      release(&pl->lk);
      yield(); // Yield the CPU to other processes
      // Re-acquire the spinlock to re-check the condition
      acquire(&pl->lk);
      // Ensure reads are fresh after yielding and re-acquiring
      __sync_synchronize();
  }

  // If the loop terminates, this process has acquired the lock.
  // Final check if the lock is still active.
  if (!pl->active) {
      release(&pl->lk);
      return -1; // Lock destroyed just before acquisition completed
  }

  // Release the internal spinlock before returning to user space.
  // The Peterson variables (flag/turn) now enforce mutual exclusion.
  release(&pl->lk);

  return 0; // Success
}

// System call: Release a Peterson lock
uint64
sys_peterson_release(void)
{
  int lock_id, role;
  struct peterson_lock *pl;

  // Get arguments
  // Fetch arguments first
  argint(0, &lock_id);
  argint(1, &role);

  // Validate arguments *after* fetching
  if(lock_id < 0 || lock_id >= NPETERSONLOCKS || (role != 0 && role != 1)) {
    return -1;
  }

  pl = &ptable[lock_id]; // Get pointer to the lock

  acquire(&pl->lk); // Acquire the internal spinlock

  // Check if the lock is active
  if (!pl->active) {
    release(&pl->lk);
    return -1;
  }

  // Peterson Algorithm: Revoke interest
  pl->flag[role] = 0;
  __sync_synchronize(); // Memory barrier after writing flag

  release(&pl->lk); // Release the internal spinlock

  return 0; // Success
}

// System call: Destroy a Peterson lock
uint64
sys_peterson_destroy(void)
{
  int lock_id;
  struct peterson_lock *pl;

  // Get argument
  // Fetch argument first
  argint(0, &lock_id);

  // Validate argument *after* fetching
  if(lock_id < 0 || lock_id >= NPETERSONLOCKS) {
    return -1;
  }

  pl = &ptable[lock_id]; // Get pointer to the lock

  acquire(&ptable_lock); // Acquire global table lock first
  acquire(&pl->lk);      // Then acquire the lock's internal spinlock

  // Check if the lock is active
  if (!pl->active) {
    // Lock already inactive or invalid
    release(&pl->lk);
    release(&ptable_lock);
    return -1;
  }

  // Free the lock (mark as inactive and reset state)
  peterson_free(lock_id);

  release(&pl->lk);      // Release internal spinlock
  release(&ptable_lock); // Release global table lock

  return 0; // Success
}
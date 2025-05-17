// user/tournament.c

#include "kernel/types.h" // Not strictly needed if user.h pulls it
#include "user/user.h"
// #include "kernel/param.h" // For NPROC, if needed for additional checks

int
main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(2, "Usage: tournament <num_processes>\n");
        exit(1);
    }

    int num_processes = atoi(argv[1]);

    if (num_processes <= 0 || num_processes > 16) {
        fprintf(2, "Number of processes must be between 1 and 16 (inclusive).\n");
        exit(1);
    }

    // Check if num_processes is a power of 2 (or 1)
    // The library's tournament_create also checks this, but an early client-side check is good.
    if (num_processes != 1 && (num_processes & (num_processes - 1)) != 0) {
        fprintf(2, "Number of processes must be a power of 2 (e.g., 1, 2, 4, 8, 16).\n");
        exit(1);
    }

    int tournament_id = tournament_create(num_processes);

    if (tournament_id < 0) {
        fprintf(2, "Process PID %d (original caller): tournament_create failed.\n", getpid());
        exit(1);
    }

    // All processes (original parent and forked children) will execute from here onwards
    // with their respective tournament_id.

    if (tournament_acquire() < 0) {
        fprintf(2, "Process PID %d (Tournament ID %d): tournament_acquire() failed.\n", getpid(), tournament_id);
        exit(1); // Exit if lock acquisition fails
    }

    // --- Critical Section ---
    printf("Process PID %d (Tournament ID %d) has acquired the lock and is in the critical section.\n",
           getpid(), tournament_id);
    // You can add a small delay here if you want to visually check for non-overlapping prints,
    // e.g., by making the process sleep for a short duration or perform some computation.
    // For xv6, a simple loop for delay:
    // for(volatile int k = 0; k < 1000000; k++);
    // --- End of Critical Section ---

    if (tournament_release() < 0) {
        fprintf(2, "Process PID %d (Tournament ID %d): tournament_release() failed.\n", getpid(), tournament_id);
        exit(1); // Exit if lock release fails
    }

    // The original process (tournament_id == 0) should wait for all children it forked.
    // Children (tournament_id > 0) will simply exit after releasing the lock.
    if (tournament_id == 0) {
        for (int i = 0; i < num_processes - 1; i++) { // Wait for (num_processes - 1) children
            if (wait((int*)0) < 0) { // In xv6, wait(0) or wait((int*)0) means don't care about status.
                fprintf(2, "Process PID %d (Tournament ID %d): wait() failed for a child.\n", getpid(), tournament_id);
                // Continue waiting for other children if possible.
            }
        }
        // printf("Process PID %d (Tournament ID %d): All children finished. Exiting.\n", getpid(), tournament_id);
    }

    exit(0);
}
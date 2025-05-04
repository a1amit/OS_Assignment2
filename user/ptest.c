//Task 1 test
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(void) {
    printf("Starting Peterson lock test...\n");

    int lock_id = peterson_create();

    if (lock_id < 0) {
        fprintf(2, "Failed to create lock\n");
        exit(1);
    }
    printf("Created lock with id: %d\n", lock_id);

    int fork_ret = fork();

    if (fork_ret < 0) {
         fprintf(2, "Fork failed\n");
         peterson_destroy(lock_id); // Attempt cleanup
         exit(1);
    }

    int role = (fork_ret > 0) ? 0 : 1; // Parent is role 0, Child is role 1

    for (int i = 0; i < 100; i++) { // might wanna change for less than 100 iterations for a faster test
        printf("Process %d (role %d) attempting to acquire lock %d...\n", getpid(), role, lock_id);
        if (peterson_acquire(lock_id, role) < 0) {
            fprintf(2, "Process %d (role %d) failed to acquire lock\n", getpid(), role);
            // If parent fails, child might still run. Need careful exit.
             if(fork_ret > 0) wait(0); // Parent waits if acquire fails
             if(role == 0) peterson_destroy(lock_id); // Try to destroy if parent fails
             exit(1);
        }

        // --- Critical section ---
        if (role == 0) {
            printf("Parent process (PID %d) in critical section (iteration %d)\n", getpid(), i);
        } else {
            printf("Child process (PID %d) in critical section (iteration %d)\n", getpid(), i);
        }
        // Simulate work
        sleep(5); // Sleep for a short time (e.g., 5 ticks)
        // --- End Critical section ---


        printf("Process %d (role %d) releasing lock %d...\n", getpid(), role, lock_id);
        if (peterson_release(lock_id, role) < 0) {
            fprintf(2, "Process %d (role %d) failed to release lock\n", getpid(), role);
             // If parent fails release, child might be stuck.
             if(fork_ret > 0) wait(0);
             if(role == 0) peterson_destroy(lock_id);
             exit(1);
        }
        // Small delay to encourage context switching
         sleep(1);
    }

    // Parent waits for child and then destroys the lock
    if (fork_ret > 0) {
        printf("Parent waiting for child...\n");
        wait(0);
        printf("Parent process destroying lock %d\n", lock_id);
        if (peterson_destroy(lock_id) < 0) {
            fprintf(2, "Parent failed to destroy lock\n");
            exit(1);
        }
         printf("Lock destroyed by parent.\n");
    } else {
         printf("Child finished.\n");
    }


    printf("Process %d finished successfully.\n", getpid());
    exit(0);
}
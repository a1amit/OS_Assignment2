// user/libtournament.c

#include "user/user.h" // For malloc, fork, peterson_*, printf, exit, wait, fprintf
// Note: "kernel/types.h" is typically included via "user/user.h" in xv6

// Global static variables for the tournament tree library
static int *peterson_lock_ids_array = 0; // Array of Peterson lock IDs, BFS order
static int num_total_locks = 0;          // N_processes - 1 locks
static int N_processes = 0;              // Number of processes in the tournament
static int L_levels = 0;                 // log2(N_processes), number of lock levels
static int my_id = -1;                   // Tournament ID of the current process (0 to N-1)

// Helper function to calculate log2(n) if n is a power of 2.
// Returns log2(n) if n is a positive power of 2. Otherwise returns -1.
static int
get_log2_if_power_of_2(int n) {
    if (n <= 0) {
        return -1; // Must be positive
    }
    if (n == 1) {
        return 0; // log2(1) = 0 levels of locks
    }
    // Check if n is a power of 2
    if ((n & (n - 1)) != 0) {
        return -1; // Not a power of 2
    }

    int log_val = 0;
    int temp = n;
    // Calculate log2 for n > 1 and power of 2
    while (temp > 1) {
        temp >>= 1;
        log_val++;
    }
    return log_val;
}

// Creates a new tournament tree for 'processes' number of processes.
// 'processes' must be a power of 2, up to 16.
// Returns the tournament ID (0 to processes-1) to each participating process.
// Returns -1 on error.
int tournament_create(int processes) {
    if (processes > 16) {
        fprintf(2, "tournament_create: Number of processes %d exceeds maximum of 16.\n", processes);
        return -1;
    }

    int temp_L_levels = get_log2_if_power_of_2(processes);
    if (temp_L_levels == -1) {
        fprintf(2, "tournament_create: Number of processes %d must be a positive power of 2.\n", processes);
        return -1;
    }

    // Set global library state. These are copied to children upon fork.
    L_levels = temp_L_levels;
    N_processes = processes;
    num_total_locks = N_processes - 1;

    if (num_total_locks > 0) {
        peterson_lock_ids_array = (int *)malloc(num_total_locks * sizeof(int));
        if (peterson_lock_ids_array == 0) {
            fprintf(2, "tournament_create: malloc failed for Peterson lock ID array.\n");
            return -1; // No cleanup specified in assignment
        }

        for (int i = 0; i < num_total_locks; i++) {
            peterson_lock_ids_array[i] = peterson_create();
            if (peterson_lock_ids_array[i] < 0) {
                fprintf(2, "tournament_create: peterson_create() failed for lock index %d.\n", i);
                // No cleanup of previously created locks or malloced memory as per assignment spec.
                return -1;
            }
        }
    } else {
        // N_processes == 1, no locks are needed.
        peterson_lock_ids_array = 0;
    }

    my_id = 0; // The initial calling process gets tournament ID 0.

    // Fork child processes. Each child will get a unique tournament ID.
    for (int i = 1; i < N_processes; i++) {
        int pid = fork();
        if (pid < 0) {
            fprintf(2, "tournament_create: fork() failed while creating process for tournament ID %d.\n", i);
            // The parent (ID 0) returns -1. Children already successfully forked will continue.
            // No cleanup as per assignment spec.
            return -1;
        }
        if (pid == 0) { // This is the child process
            my_id = i;
            // Globals N_processes, L_levels, num_total_locks, and peterson_lock_ids_array
            // are inherited from the parent.
            return my_id; // Child returns its assigned tournament ID
        }
        // Parent (my_id == 0) continues to fork the next child.
    }

    // The original parent (ID 0) or the only process (if N_processes=1) returns its ID.
    return my_id;
}

// Attempts to acquire the root lock of the tournament tree for the calling process.
// Returns 0 on success, -1 on error.
int tournament_acquire(void) {
    if (my_id == -1) {
        fprintf(2, "tournament_acquire: Process (PID %d) not part of an initialized tournament (my_id is -1).\n", getpid());
        return -1;
    }
    if (N_processes <= 0) { // Should be caught by create, but defensive check.
         fprintf(2, "tournament_acquire: N_processes (%d) is invalid.\n", N_processes);
         return -1;
    }
    if (N_processes == 1) {
        return 0; // Trivial acquisition if only one process in the tournament.
    }
    if (num_total_locks > 0 && peterson_lock_ids_array == 0) {
        fprintf(2, "tournament_acquire: Lock array not initialized despite N_processes > 1.\n");
        return -1;
    }

    // Acquire Peterson locks from the bottom-most level (L_levels - 1) up to the root (level 0).
    for (int l = L_levels - 1; l >= 0; l--) {
        // Role for this process at level 'l': extract the (L_levels - 1 - l)-th bit of my_id.
        int role_bit_position = L_levels - 1 - l;
        int role_at_level = (my_id >> role_bit_position) & 1;

        // Index of the lock within its level 'l': my_id shifted by (L_levels - l).
        int lock_group_shift = L_levels - l;
        int lock_idx_within_level = my_id >> lock_group_shift;

        // Overall index in the BFS-ordered peterson_lock_ids_array.
        // (1 << l) - 1 is the sum of locks in all levels above 'l'.
        int offset_for_level_nodes = (1 << l) - 1;
        int array_idx = lock_idx_within_level + offset_for_level_nodes;

        if (array_idx < 0 || array_idx >= num_total_locks) {
            fprintf(2, "tournament_acquire: (PID %d, id %d, L%d) - Calculated lock array index %d is out of bounds (0 to %d).\n",
                   getpid(), my_id, l, array_idx, num_total_locks - 1);
            return -1; // This indicates an internal logic error.
        }

        int lock_id_to_acquire = peterson_lock_ids_array[array_idx];

        if (peterson_acquire(lock_id_to_acquire, role_at_level) < 0) {
            fprintf(2, "tournament_acquire: (PID %d, id %d, L%d) - Failed to acquire Peterson lock (kernel ID %d, array_idx %d) with role %d.\n",
                   getpid(), my_id, l, lock_id_to_acquire, array_idx, role_at_level);
            // No rollback of already acquired locks is specified.
            return -1;
        }
    }
    return 0; // Successfully acquired all locks up to the root.
}

// Releases all locks held by the calling process in the reverse order of acquisition.
// Returns 0 on success, -1 on error.
int tournament_release(void) {
    if (my_id == -1) {
        fprintf(2, "tournament_release: Process (PID %d) not part of an initialized tournament (my_id is -1).\n", getpid());
        return -1;
    }
    if (N_processes <= 0) {
         fprintf(2, "tournament_release: N_processes (%d) is invalid.\n", N_processes);
         return -1;
    }
    if (N_processes == 1) {
        return 0; // Trivial release if only one process.
    }
    if (num_total_locks > 0 && peterson_lock_ids_array == 0) {
        fprintf(2, "tournament_release: Lock array not initialized despite N_processes > 1.\n");
        return -1;
    }

    // Release Peterson locks from the root (level 0) down to the bottom-most level (L_levels - 1).
    for (int l = 0; l < L_levels; l++) {
        int role_bit_position = L_levels - 1 - l;
        int role_at_level = (my_id >> role_bit_position) & 1;

        int lock_group_shift = L_levels - l;
        int lock_idx_within_level = my_id >> lock_group_shift;

        int offset_for_level_nodes = (1 << l) - 1;
        int array_idx = lock_idx_within_level + offset_for_level_nodes;

        if (array_idx < 0 || array_idx >= num_total_locks) {
             fprintf(2, "tournament_release: (PID %d, id %d, L%d) - Calculated lock array index %d is out of bounds (0 to %d).\n",
                   getpid(), my_id, l, array_idx, num_total_locks - 1);
            return -1; // Internal logic error.
        }

        int lock_id_to_release = peterson_lock_ids_array[array_idx];

        if (peterson_release(lock_id_to_release, role_at_level) < 0) {
            fprintf(2, "tournament_release: (PID %d, id %d, L%d) - Failed to release Peterson lock (kernel ID %d, array_idx %d) with role %d.\n",
                   getpid(), my_id, l, lock_id_to_release, array_idx, role_at_level);
            // Assignment does not specify behavior on partial failure.
            return -1;
        }
    }
    return 0; // Successfully released all locks.
}
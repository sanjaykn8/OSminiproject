#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <signal.h>

// PID file path
#define PID_FILE "/tmp/producer_pids"

// Function to clean up the PID from the PID file
void remove_pid(pid_t pid) {
    FILE *fp = fopen(PID_FILE, "r+");
    if (!fp) {
        fprintf(stderr, "Failed to open PID file for cleanup: %s\n", strerror(errno));
        return;
    }

    // Temporary buffer to store all PIDs except the current one
    char buffer[1024];
    size_t size = 0;
    pid_t temp_pid;

    // Read all PIDs and rebuild the file without the current PID
    while (fscanf(fp, "%d\n", &temp_pid) == 1) {
        if (temp_pid != pid) {
            size += snprintf(buffer + size, sizeof(buffer) - size, "%d\n", temp_pid);
        }
    }

    // Truncate and overwrite the PID file with the updated content
    freopen(PID_FILE, "w", fp);
    fwrite(buffer, 1, size, fp);
    fclose(fp);
}

// Signal handler for cleanup on termination
void cleanup(int signum) {
    pid_t pid = getpid();
    printf("Cleaning up PID %d...\n", pid);
    remove_pid(pid);
    exit(0);
}

int main(int argc, char *argv[]) {
    const char *name = argc > 1 ? argv[1] : "Producer";

    // Register signal handler for termination (SIGTERM, SIGINT, etc.)
    signal(SIGTERM, cleanup);
    signal(SIGINT, cleanup);

    // Register own PID in the PID file
    FILE *fp = fopen(PID_FILE, "a");
    if (!fp) {
        fprintf(stderr, "Failed to open PID file %s: %s\n", PID_FILE, strerror(errno));
        exit(EXIT_FAILURE);
    }
    pid_t pid = getpid();
    fprintf(fp, "%d\n", pid);
    fclose(fp);

    // Simulate work
    while (1) {
        printf("%s (PID %d): working...\n", name, pid);
        fflush(stdout);
        sleep(1);
    }

    return 0;
}


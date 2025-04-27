#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <signal.h>

// PID file path and pipe path
#define PID_FILE "/tmp/producer_pids"
#define PIPE_PATH "/tmp/producer_pipe"

void remove_pid(pid_t pid) {
    FILE *fp = fopen(PID_FILE, "r+");
    if (!fp) {
        fprintf(stderr, "Failed to open PID file for cleanup: %s\n", strerror(errno));
        return;
    }

    char buffer[4096];
    size_t size = 0;
    pid_t temp_pid;

    while (fscanf(fp, "%d\n", &temp_pid) == 1) {
        if (temp_pid != pid) {
            size += snprintf(buffer + size, sizeof(buffer) - size, "%d\n", temp_pid);
        }
    }

    freopen(PID_FILE, "w", fp);
    fwrite(buffer, 1, size, fp);
    fclose(fp);
}

void cleanup(int signum) {
    pid_t pid = getpid();
    printf("Cleaning up PID %d...\n", pid);
    remove_pid(pid);
    exit(0);
}

int main(int argc, char *argv[]) {
    const char *name = argc > 1 ? argv[1] : "Producer";

    signal(SIGTERM, cleanup);
    signal(SIGINT, cleanup);

    FILE *fp = fopen(PID_FILE, "a");
    if (!fp) {
        fprintf(stderr, "Failed to open PID file %s: %s\n", PID_FILE, strerror(errno));
        exit(EXIT_FAILURE);
    }
    pid_t pid = getpid();
    fprintf(fp, "%d\n", pid);
    fclose(fp);

    // Open the pipe
    int pipe_fd;
    while ((pipe_fd = open(PIPE_PATH, O_WRONLY)) == -1) {
        perror("Waiting for manager to create pipe");
        sleep(1);
    }

    int counter = 0;
    while (1) {
        dprintf(pipe_fd, "%d\n", counter);  // send number to pipe
        printf("%s (PID %d): running (%d)\n", name, pid, counter);
        fflush(stdout);
        counter++;
        sleep(1);
    }

    close(pipe_fd);
    return 0;
}

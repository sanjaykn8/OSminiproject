#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/resource.h>
#include <errno.h>

#define SOCKET_PATH    "/tmp/manager_socket"
#define PID_FILE       "/tmp/producer_pids"
#define MAX_PRODUCERS  1024

// Read PIDs
static int read_pids(pid_t *pids) {
    FILE *fp = fopen(PID_FILE, "r");
    if (!fp) return 0;
    int count = 0;
    while (count < MAX_PRODUCERS && fscanf(fp, "%d\n", &pids[count]) == 1) count++;
    fclose(fp);
    return count;
}

// Get state
static char get_state(pid_t pid) {
    char path[64], buf[256];
    snprintf(path, sizeof(path), "/proc/%d/stat", pid);
    FILE *fp = fopen(path, "r");
    if (!fp) return '?';
    fscanf(fp, "%*d (%*[^)]) %c", &buf[0]);
    fclose(fp);
    return buf[0];
}

static const char* state_str(char s) {
    switch (s) {
        case 'R': return "Running";
        case 'S': return "Sleeping";
        case 'D': return "Uninterruptible";
        case 'T': case 't': return "Stopped";
        case 'Z': return "Zombie";
        default:  return "Unknown";
    }
}

// Handle one client
static void handle_client(int client) {
    FILE *in = fdopen(client, "r");
    FILE *out = fdopen(dup(client), "w");
    if (!in || !out) { close(client); return; }

    char line[128];
    while (fgets(line, sizeof(line), in)) {
        char *cmd = strtok(line, " \t\n");
        if (!cmd) continue;

        if (strcmp(cmd, "LIST") == 0) {
            pid_t pids[MAX_PRODUCERS];
            int n = read_pids(pids);
            for (int i = 0; i < n; i++) {
                if (kill(pids[i], 0) == -1 && errno == ESRCH) continue;
                char s = get_state(pids[i]);
                int prio = getpriority(PRIO_PROCESS, pids[i]);
                fprintf(out, "%d %s %d\n", pids[i], state_str(s), prio);
            }
            fprintf(out, "END\n");
            fflush(out);
        }
        else if (strcmp(cmd, "PAUSE") == 0 || strcmp(cmd, "RESUME") == 0 || strcmp(cmd, "KILL") == 0) {
            pid_t pid = (pid_t)atoi(strtok(NULL, " \t\n"));
            int sig = strcmp(cmd, "PAUSE") == 0 ? SIGSTOP :
                      strcmp(cmd, "RESUME") == 0 ? SIGCONT : SIGKILL;
            if (kill(pid, sig) == 0) fprintf(out, "OK\n");
            else fprintf(out, "ERROR %s\n", strerror(errno));
            fflush(out);
        }
        else if (strcmp(cmd, "PRIORITY") == 0) {
            pid_t pid = (pid_t)atoi(strtok(NULL, " \t\n"));
            int pr = atoi(strtok(NULL, " \t\n"));
            if (setpriority(PRIO_PROCESS, pid, pr) == 0) fprintf(out, "OK\n");
            else fprintf(out, "ERROR %s\n", strerror(errno));
            fflush(out);
        }
    }
    fclose(in);
    fclose(out);
}

int main(void) {
    signal(SIGPIPE, SIG_IGN);  // Don't crash if client disconnects

    unlink(SOCKET_PATH);
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) { perror("socket"); exit(1); }

    struct sockaddr_un addr = { .sun_family = AF_UNIX };
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        exit(1);
    }

    chmod(SOCKET_PATH, 0666);

    if (listen(sock, 5) < 0) {
        perror("listen");
        exit(1);
    }

    printf("Manager listening on %s\n", SOCKET_PATH);

    while (1) {
        int client = accept(sock, NULL, NULL);
        if (client >= 0) {
            pid_t pid = fork();
            if (pid == 0) { // child
                close(sock);
                handle_client(client);
                close(client);
                exit(0);
            }
            close(client); // parent
        }
    }
}


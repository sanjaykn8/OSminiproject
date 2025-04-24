// manager.c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ncurses.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <time.h>

#define MAX_PRODUCERS 5
#define STATUS_FILE "consumer_status.txt"

typedef enum { RUNNING, PAUSED, KILLED } ProcState;

pid_t producers[MAX_PRODUCERS];
ProcState states[MAX_PRODUCERS];
int current = 0;

void spawn_producer(int i) {
    pid_t pid = fork();
    if (pid == 0) {
        char arg[10];
        sprintf(arg, "%d", i);
        execl("./producer", "producer", arg, NULL);
        perror("exec failed");
        exit(1);
    } else {
        producers[i] = pid;
        states[i] = RUNNING;
    }
}

void kill_producer(int i) {
    if (producers[i] > 0) {
        kill(producers[i], SIGTERM);
        waitpid(producers[i], NULL, 0);
        producers[i] = -1;
        states[i] = KILLED;
    }
}

void pause_producer(int i) {
    if (producers[i] > 0 && states[i] == RUNNING) {
        kill(producers[i], SIGSTOP);
        states[i] = PAUSED;
    }
}

void resume_producer(int i) {
    if (producers[i] > 0 && states[i] == PAUSED) {
        kill(producers[i], SIGCONT);
        states[i] = RUNNING;
    }
}

const char* state_str(ProcState state) {
    switch (state) {
        case RUNNING: return "Running";
        case PAUSED:  return "Paused ";
        case KILLED:  return "Killed ";
        default: return "Unknown";
    }
}

void draw_ui() {
    clear();
    mvprintw(0, 2, "Process Manager");

    for (int i = 0; i < MAX_PRODUCERS; ++i) {
        mvprintw(i + 2, 2, "Producer %d [PID: %d] [%s]%s", 
                 i, producers[i], state_str(states[i]),
                 (i == current) ? "  <--" : "");
    }

    // Read consumer status from file
    FILE *fp = fopen(STATUS_FILE, "r");
    char buffer[128] = "No data";
    if (fp) {
        fgets(buffer, sizeof(buffer), fp);
        fclose(fp);
    }

    mvprintw(MAX_PRODUCERS + 4, 2, "Consumer Status:");
    mvprintw(MAX_PRODUCERS + 5, 4, "%s", buffer);
    mvprintw(MAX_PRODUCERS + 7, 2, "Controls: ↑↓ to select | p:pause | r:resume | k:kill | q:quit");
    refresh();
}

int main() {
    initscr();
    noecho();
    cbreak();
    keypad(stdscr, TRUE);

    for (int i = 0; i < MAX_PRODUCERS; ++i)
        spawn_producer(i);

    int ch;
    while (1) {
        draw_ui();
        ch = getch();
        switch (ch) {
            case 'q':
                for (int i = 0; i < MAX_PRODUCERS; ++i)
                    kill_producer(i);
                endwin();
                return 0;
            case 'p':
                pause_producer(current);
                break;
            case 'r':
                resume_producer(current);
                break;
            case 'k':
                kill_producer(current);
                break;
            case KEY_UP:
                if (current > 0) current--;
                break;
            case KEY_DOWN:
                if (current < MAX_PRODUCERS - 1) current++;
                break;
        }
    }
}


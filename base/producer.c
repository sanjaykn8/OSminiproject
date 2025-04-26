// File: producer.c
// Compile with: gcc producer.c -o producer -lrt

#include <stdio.h>
#include <stdlib.h>
#include <mqueue.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <sys/stat.h>

#define MQ_NAME  "/prod_cons_mq"

struct message {
    int producer_id;
    int value;
};

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <producer_id>\n", argv[0]);
        return 1;
    }
    int id = atoi(argv[1]);

    mqd_t mq = mq_open(MQ_NAME, O_WRONLY);
    if (mq == (mqd_t)-1) {
        perror("mq_open (producer)");
        return 1;
    }

    srand(time(NULL) ^ (getpid()<<16));

    while (1) {
        struct message msg;
        msg.producer_id = id;
        msg.value = rand() % 1000;   // generate some “different” values

        if (mq_send(mq, (const char*)&msg, sizeof(msg), 0) == -1) {
            perror("mq_send");
        } else {
            printf("[Producer %d] sent value %d\n", id, msg.value);
        }

        sleep(2);
    }

    mq_close(mq);
    return 0;
}


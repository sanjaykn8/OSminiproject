// consumer.c
#include <stdio.h>
#include <stdlib.h>
#include <mqueue.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#define MQ_NAME  "/prod_cons_mq"
#define STATUS_FILE "consumer_status.txt"

struct message {
    int producer_id;
    int value;
};

mqd_t mq;

void cleanup(int sig) {
    mq_close(mq);
    mq_unlink(MQ_NAME);
    printf("\n[Consumer] Exiting.\n");
    exit(0);
}

void log_to_file(int producer_id, int value) {
    FILE *fp = fopen(STATUS_FILE, "w");
    if (fp) {
        fprintf(fp, "Last value: %d (from producer %d)\n", value, producer_id);
        fclose(fp);
    }
}

int main() {
    struct mq_attr attr = {
        .mq_flags = 0,
        .mq_maxmsg = 10,
        .mq_msgsize = sizeof(struct message),
        .mq_curmsgs = 0
    };

    signal(SIGINT, cleanup);

    mq = mq_open(MQ_NAME, O_CREAT | O_RDONLY, 0644, &attr);
    if (mq == (mqd_t)-1) {
        perror("mq_open (consumer)");
        return 1;
    }

    printf("[Consumer] Listening...\n");

    while (1) {
        struct message msg;
        ssize_t bytes = mq_receive(mq, (char*)&msg, sizeof(msg), NULL);
        if (bytes >= 0) {
            printf("[Consumer] Got %d from producer %d\n", msg.value, msg.producer_id);
            log_to_file(msg.producer_id, msg.value);
        } else {
            perror("mq_receive");
            sleep(1);
        }
    }

    return 0;
}


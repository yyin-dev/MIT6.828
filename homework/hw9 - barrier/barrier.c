#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/*
pthread_mutex_t lock;               // declare a lock
pthread_mutex_init(&lock, NULL);    // initialize the lock
pthread_mutex_lock(&lock);          // acquire lock
pthread_mutex_unlock(&lock);        // release lock
pthread_cond_wait(&cond, &mutex);   // go to sleep on cond, releasing lock mutex
pthread_cond_broadcast(&cond);      // wake up every thread sleeping on cond
*/

static int nthread = 1;
static int round = 0;

struct barrier {
    pthread_mutex_t barrier_mutex;
    pthread_cond_t barrier_cond;
    int nthread;  // Number of threads that have reached this round of the barrier
    int round;    // Barrier round
} bstate;

static void
barrier_init(void) {
    assert(pthread_mutex_init(&bstate.barrier_mutex, NULL) == 0);
    assert(pthread_cond_init(&bstate.barrier_cond, NULL) == 0);
    bstate.nthread = 0;
}

static void
barrier() {
    pthread_mutex_lock(&bstate.barrier_mutex);
    bstate.nthread += 1;
    if (bstate.nthread == nthread) {
        bstate.nthread = 0;
        bstate.round += 1;
        pthread_cond_broadcast(&bstate.barrier_cond);
    } else {
        pthread_cond_wait(&bstate.barrier_cond, &bstate.barrier_mutex);
    }
    pthread_mutex_unlock(&bstate.barrier_mutex);
}

static void *
thread(void *xa) {
    long n = (long)xa;
    long delay;
    int i;

    for (i = 0; i < 20000; i++) {
        int t = bstate.round;
        assert(i == t);
        barrier();
        usleep(random() % 100);
    }
}

int main(int argc, char *argv[]) {
    pthread_t *pid_array;
    void *value;
    long i;
    double t1, t0;

    if (argc < 2) {
        fprintf(stderr, "%s: %s nthread\n", argv[0], argv[0]);
        exit(-1);
    }

    nthread = atoi(argv[1]);
    pid_array = malloc(sizeof(pthread_t) * nthread);
    srandom(0);

    barrier_init();

    for (i = 0; i < nthread; i++) {
        assert(pthread_create(&pid_array[i], NULL, thread, (void *)i) == 0);
    }

    for (i = 0; i < nthread; i++) {
        assert(pthread_join(pid_array[i], &value) == 0);
    }

    printf("OK; passed\n");
}

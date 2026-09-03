/* Measures how long a cancel actually takes to stop the engine.
 * Starts a large computation, flips the cancel flag after ~1s, and reports
 * the delay until the worker returns. */
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include "../core/pi_engine.h"

static pi_progress_t prog;
static long g_terms;

static double now_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

static void *worker(void *arg) {
    (void)arg;
    pi_bs_t r;
    pi_bs_init(&r);
    pi_bs_compute(0, g_terms, &r, &prog);
    pi_bs_clear(&r);
    return NULL;
}

int main(int argc, char **argv) {
    long digits = argc > 1 ? atol(argv[1]) : 50000000;
    g_terms = pi_terms_for_digits(digits);
    prog.leaves_total = g_terms;
    prog.leaves_done = 0;
    prog.cancel = 0;

    printf("target: %ld digits (%ld terms)\n", digits, g_terms);

    pthread_t th;
    double t0 = now_sec();
    pthread_create(&th, NULL, worker, NULL);

    struct timespec pause = {1, 0};
    nanosleep(&pause, NULL);

    double tCancel = now_sec();
    printf("cancelling at %.2fs (progress %.1f%%)\n", tCancel - t0,
           100.0 * (double)prog.leaves_done / (double)prog.leaves_total);
    prog.cancel = 1;

    pthread_join(th, NULL);
    double tDone = now_sec();

    printf("stopped %.3fs after cancel (total %.2fs)\n", tDone - tCancel, tDone - t0);
    return 0;
}

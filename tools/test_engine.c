#include <stdio.h>
#include <stdlib.h>
#include "../core/pi_engine.h"

int main(int argc, char **argv) {
    long digits = argc > 1 ? atol(argv[1]) : 100;
    pi_progress_t prog = {0, 0, 0};
    char *out = NULL;
    int rc = pi_compute(digits, &prog, &out);
    if (rc != 0) {
        fprintf(stderr, "pi_compute failed: %d\n", rc);
        return 1;
    }
    printf("%s\n", out);
    fprintf(stderr, "leaves: %ld/%ld\n", prog.leaves_done, prog.leaves_total);
    pi_free(out);
    return 0;
}

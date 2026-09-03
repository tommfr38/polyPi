#include <stdio.h>
#include <string.h>
#include "pi_engine.h"

int main() {
    pi_progress_t prog = {0,0,0};
    char *out = NULL;
    pi_compute(1000000, &prog, &out);
    printf("len=%d\n", (int)strlen(out));
    printf("head=%.60s\n", out);
    pi_free(out);
    return 0;
}

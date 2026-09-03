#include <stdlib.h>
#include <string.h>
#include "pi_engine.h"

/* Thin export surface for the WASM build: the browser only ever asks for a
 * one-shot, single-threaded compute (the web tier is capped low enough that
 * threading isn't needed - see pi_compute benchmarks). */

char *wasm_compute_pi(int digits) {
    pi_progress_t prog = {0, 0, 0};
    char *out = NULL;
    int rc = pi_compute((long)digits, &prog, &out);
    if (rc != 0) return NULL;
    return out;
}

void wasm_free(char *s) {
    pi_free(s);
}

#ifndef PI_ENGINE_H
#define PI_ENGINE_H

#include <gmp.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Which stage a run is in. Counting is interruptible almost instantly;
 * finalizing is a handful of big opaque GMP calls, so a cancel there only
 * takes effect at the next stage boundary. */
enum {
    PI_PHASE_COUNTING = 0,
    PI_PHASE_FINALIZING = 1
};

typedef struct {
    volatile long leaves_done;
    long leaves_total;
    volatile int cancel;
    volatile int phase;
} pi_progress_t;

typedef struct {
    mpz_t P, Q, T;
} pi_bs_t;

void pi_bs_init(pi_bs_t *r);
void pi_bs_clear(pi_bs_t *r);

/* Recursively compute the Chudnovsky binary-splitting triple (P,Q,T) for
 * term range [a,b). Pure, single-threaded, reentrant across disjoint ranges
 * so a caller can run several of these concurrently on different ranges and
 * merge with pi_bs_combine. Increments progress->leaves_done per leaf if
 * progress is non-NULL; bails out early if progress->cancel becomes true. */
void pi_bs_compute(long a, long b, pi_bs_t *out, pi_progress_t *progress);

/* Merge two adjacent ranges [a,m) and [m,b) into [a,b). */
void pi_bs_combine(const pi_bs_t *left, const pi_bs_t *right, pi_bs_t *out);

/* How many Chudnovsky terms are needed for `digits` correct decimal digits. */
long pi_terms_for_digits(long digits);

/* Turn a finished (P,Q,T) for range [0,N) into a decimal string "3.1415...."
 * with exactly `digits` digits after the point. Caller frees with pi_free.
 * `progress` may be NULL; when given, the phase is reported and the cancel
 * flag is honoured between stages, in which case NULL is returned. */
char *pi_finalize(long digits, const pi_bs_t *r, pi_progress_t *progress);

void pi_free(char *s);

/* Convenience single-threaded end-to-end compute (used by the WASM build). */
int pi_compute(long digits, pi_progress_t *progress, char **out);

#ifdef __cplusplus
}
#endif

#endif

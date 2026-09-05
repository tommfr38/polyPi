#ifndef PI_ENGINE_H
#define PI_ENGINE_H

#include <gmp.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Which stage a run is in. Counting is interruptible almost instantly.
 * Merging is a shrinking number of ever-larger multiplications, so a cancel
 * there lands at the next round boundary. Finalizing is a handful of big
 * opaque GMP calls and a cancel only takes effect between them. */
enum {
    PI_PHASE_COUNTING = 0,
    PI_PHASE_MERGING = 1,
    PI_PHASE_FINALIZING = 2
};

/* MSVC has no __atomic_* builtins; these flags only need relaxed ordering. */
#if defined(_MSC_VER)
#include <intrin.h>
#define PI_LOAD(p)     (*(volatile int *)(p))
#define PI_STORE(p, v) (*(volatile int *)(p) = (v))
#define PI_INC(p)      _InterlockedIncrement((volatile long *)(p))
#define PI_THREAD_LOCAL __declspec(thread)
#else
#define PI_LOAD(p)     __atomic_load_n((p), __ATOMIC_RELAXED)
#define PI_STORE(p, v) __atomic_store_n((p), (v), __ATOMIC_RELAXED)
#define PI_INC(p)      __atomic_add_fetch((p), 1, __ATOMIC_RELAXED)
#define PI_THREAD_LOCAL __thread
#endif

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
 * term range [a,b) into `out`, which must already be initialised. Pure,
 * single-threaded, reentrant across disjoint ranges so a caller can run
 * several of these concurrently on different ranges and merge the results
 * with pi_bs_merge. Increments progress->leaves_done per leaf if progress is
 * non-NULL; bails out early if progress->cancel becomes true. */
void pi_bs_compute(long a, long b, pi_bs_t *out, pi_progress_t *progress);

/* Same, for a range that is the whole computation - nothing will be merged
 * onto the result, so its P is never read and is not built. At a billion
 * digits that is ~700 MB and one full-size multiplication saved. Only Q and T
 * of the result are meaningful. */
void pi_bs_compute_root(long a, long b, pi_bs_t *out, pi_progress_t *progress);

/* Merge the adjacent range `right` into `left`, in place: [a,m) . [m,b).
 *
 * In place because the operands are the largest objects in the program by an
 * order of magnitude - writing to a third triple means holding 1.5x the
 * result at the point where memory is already tightest. Each half of `right`
 * is released as soon as it has been consumed, for the same reason.
 *
 * Pass need_p = 0 when nothing further will be merged onto `left`, i.e. this
 * is the final merge of the run; the combined P is then skipped entirely.
 *
 * `right` is left valid but empty, so the owner still clears it as usual. */
void pi_bs_merge(pi_bs_t *left, pi_bs_t *right, int need_p);

/* How many Chudnovsky terms are needed for `digits` correct decimal digits. */
long pi_terms_for_digits(long digits);

/* Largest digit count this build can actually represent. GMP's mp_bitcnt_t
 * is `unsigned long`, so on Windows it is 32-bit: somewhere past 1.29 billion
 * digits the working precision no longer fits it, wraps, and the run returns
 * nonsense instead of failing. Callers should clamp to this. */
long pi_max_digits(void);

/* Turn a finished (P,Q,T) for range [0,N) into "3.1415...." with exactly
 * `digits` digits after the point, written into `buf` - which the caller
 * supplies and which must hold at least pi_result_size(digits) bytes. Taking
 * the buffer from the caller avoids formatting into one gigabyte-scale buffer
 * only to copy it into another.
 *
 * `progress` may be NULL; when given, the phase is reported and the cancel
 * flag is honoured between stages. Returns 0 on success and non-zero if the
 * run was cancelled or the conversion failed. Consumes `r`: it is cleared
 * here, so the caller must not clear it again. */
int pi_finalize_into(long digits, const pi_bs_t *r, pi_progress_t *progress, char *buf);

/* Bytes pi_finalize_into needs. The result itself is digits+2 characters. */
size_t pi_result_size(long digits);

/* Allocating wrapper around pi_finalize_into. Returns NULL if the buffer
 * could not be allocated or pi_finalize_into failed. Frees with pi_free. */
char *pi_finalize(long digits, const pi_bs_t *r, pi_progress_t *progress);

void pi_free(char *s);

/* Convenience single-threaded end-to-end compute (used by the WASM build). */
int pi_compute(long digits, pi_progress_t *progress, char **out);

#ifdef __cplusplus
}
#endif

#endif

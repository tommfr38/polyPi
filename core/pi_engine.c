#include "pi_engine.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#define CHUD_A 13591409UL
#define CHUD_B 545140134UL
#define CHUD_C 640320UL

/* bits of working precision per decimal digit: log2(10) */
#define PI_BITS_PER_DIGIT 3.3219281
/* digits per Chudnovsky term: log10(640320^3 / 24) / 2 ~= 14.1816 */
#define PI_DIGITS_PER_TERM 14.1816

long pi_terms_for_digits(long digits) {
    long terms = (long)(digits / PI_DIGITS_PER_TERM) + 2;
    return terms < 1 ? 1 : terms;
}

long pi_max_digits(void) {
    /* mp_bitcnt_t is `unsigned long`: 64-bit on macOS/Linux, 32-bit on
     * Windows. Past the point where the working precision stops fitting it,
     * the cast in pi_finalize_into wraps and the run quietly returns
     * nonsense instead of failing. */
    mp_bitcnt_t max_prec = (mp_bitcnt_t)-1;
    double from_prec = ((double)max_prec - 256.0) / PI_BITS_PER_DIGIT;
    /* the leaf below evaluates 6*a in `long`, which is also 32-bit there */
    double from_terms = (double)(LONG_MAX / 6) * PI_DIGITS_PER_TERM;
    double cap = from_prec < from_terms ? from_prec : from_terms;
    if (cap > (double)(LONG_MAX - 8)) cap = (double)(LONG_MAX - 8);
    return (long)cap;
}

size_t pi_result_size(long digits) {
    /* "3." + digits + terminator is digits+3, but the formatting step hands
     * GMP the tail of this buffer and GMP asks for n_digits+2 bytes there -
     * a byte for a sign it will never write, since pi is positive. Give it
     * the byte rather than relying on that. */
    return (size_t)digits + 4;
}

void pi_bs_init(pi_bs_t *r) {
    mpz_init(r->P);
    mpz_init(r->Q);
    mpz_init(r->T);
}

void pi_bs_clear(pi_bs_t *r) {
    mpz_clear(r->P);
    mpz_clear(r->Q);
    mpz_clear(r->T);
}

/* Hand an integer's limbs back to the allocator now, leaving it valid (and
 * zero) so its owner still clears it as usual. At these sizes, waiting for
 * the enclosing pi_bs_clear means carrying a gigabyte through the next
 * multiplication for no reason. */
static void release(mpz_t x) {
    mpz_clear(x);
    mpz_init(x);
}

static void c3_over_24(mpz_t out) {
    mpz_init_set_ui(out, CHUD_C);
    mpz_pow_ui(out, out, 3);
    mpz_divexact_ui(out, out, 24);
}

void pi_bs_merge(pi_bs_t *l, pi_bs_t *r, int need_p) {
    mpz_t t;

    /* Every product goes into a fresh temporary that is swapped in, and each
     * operand is released the moment it is spent. Writing a product straight
     * into one of its own operands would read more neatly, but GMP then has
     * to allocate a hidden full-size temporary and copy out of it - a spare
     * gigabyte held for the length of the multiplication. */

    /* T = l.T*r.Q + l.P*r.T; addmul so only one of the two products exists */
    mpz_init(t);
    mpz_mul(t, l->T, r->Q);
    mpz_addmul(t, l->P, r->T);
    mpz_swap(l->T, t);
    mpz_clear(t);            /* the old l.T */
    release(r->T);

    if (need_p) {
        mpz_init(t);
        mpz_mul(t, l->P, r->P);
        mpz_swap(l->P, t);
        mpz_clear(t);
    } else {
        release(l->P);       /* nothing will be merged onto this result */
    }
    release(r->P);

    mpz_init(t);
    mpz_mul(t, l->Q, r->Q);
    mpz_swap(l->Q, t);
    mpz_clear(t);
    release(r->Q);
}

static int cancelled(const pi_progress_t *progress) {
    return progress && PI_LOAD(&progress->cancel);
}

static void set_identity(pi_bs_t *out) {
    mpz_set_ui(out->P, 1);
    mpz_set_ui(out->Q, 1);
    mpz_set_ui(out->T, 0);
}

static void bs(long a, long b, pi_bs_t *out, pi_progress_t *progress, int need_p) {
    long m;
    pi_bs_t right;

    if (cancelled(progress)) {
        set_identity(out);
        return;
    }

    if (b - a == 1) {
        static PI_THREAD_LOCAL int have_c3 = 0;
        static PI_THREAD_LOCAL mpz_t c3;
        if (!have_c3) {
            c3_over_24(c3);
            have_c3 = 1;
        }

        if (a == 0) {
            mpz_set_ui(out->P, 1);
            mpz_set_ui(out->Q, 1);
        } else {
            mpz_set_si(out->P, 6 * a - 5);
            mpz_mul_si(out->P, out->P, 2 * a - 1);
            mpz_mul_si(out->P, out->P, 6 * a - 1);
            mpz_set_si(out->Q, a);
            mpz_pow_ui(out->Q, out->Q, 3);
            mpz_mul(out->Q, out->Q, c3);
        }

        mpz_t t;
        mpz_init_set_si(t, a);
        mpz_mul_ui(t, t, CHUD_B);
        mpz_add_ui(t, t, CHUD_A);
        mpz_mul(out->T, t, out->P);
        if (a & 1) mpz_neg(out->T, out->T);
        mpz_clear(t);

        if (progress) PI_INC(&progress->leaves_done);
        return;
    }

    /* The left half is computed straight into `out` and the right half is
     * merged onto it, so a node holds two triples where it used to hold
     * three. That is the single largest saving available here: the peak is
     * always at a merge, and the merges near the root are gigabytes. */
    m = a + (b - a) / 2;
    bs(a, m, out, progress, 1);
    pi_bs_init(&right);
    bs(m, b, &right, progress, 1);
    /* The merges are where the huge multiplications live, and they happen as
     * the recursion unwinds. Without this check a cancel still has to grind
     * through one full-size mpz_mul per stack level before it takes effect. */
    if (cancelled(progress)) {
        set_identity(out);
    } else {
        pi_bs_merge(out, &right, need_p);
    }
    pi_bs_clear(&right);
}

void pi_bs_compute(long a, long b, pi_bs_t *out, pi_progress_t *progress) {
    bs(a, b, out, progress, 1);
}

void pi_bs_compute_root(long a, long b, pi_bs_t *out, pi_progress_t *progress) {
    bs(a, b, out, progress, 0);
}

int pi_finalize_into(long digits, const pi_bs_t *r, pi_progress_t *progress, char *buf) {
    /* `r` is consumed piece by piece, as each piece is converted, so an mpz
     * and its mpf copy are never both held at full size. */
    pi_bs_t *src = (pi_bs_t *)r;
    mp_bitcnt_t prec;
    mpf_t Qf, Tf, sqrtC, num, pi;
    mp_exp_t exp;
    size_t got;
    int operands_live = 1;
    int rc = -1;

    if (progress) PI_STORE(&progress->phase, PI_PHASE_FINALIZING);

    prec = (mp_bitcnt_t)(digits * PI_BITS_PER_DIGIT) + 256;
    mpf_set_default_prec(prec);

    release(src->P);   /* dead since the last merge - 700 MB at a billion digits */

    /* Convert and release one at a time: an integer and its float copy are
     * never both held, which at a billion digits is 1.8 GB not carried. */
    mpf_init(Qf);
    mpf_set_z(Qf, src->Q);
    release(src->Q);
    mpf_init(Tf);
    mpf_set_z(Tf, src->T);
    pi_bs_clear(src);
    mpf_init(sqrtC);
    mpf_init(num);
    mpf_init(pi);

    /* Each of the steps below is a single opaque GMP call, so the cancel flag
     * can only be honoured between them - the string conversion in particular
     * is the largest uninterruptible chunk of a run. Each result also goes to
     * a destination distinct from its operands: aliasing makes GMP allocate a
     * hidden temporary of the same size and copy out of it. */
    if (cancelled(progress)) goto done;
    mpf_sqrt_ui(sqrtC, 10005);
    mpf_mul(num, Qf, sqrtC);
    mpf_mul_ui(num, num, 426880);   /* by a single limb; in place is fine */

    if (cancelled(progress)) goto done;
    mpf_div(pi, num, Tf);

    /* only `pi` is needed from here on; release the rest before the string
     * conversion rather than making it compete with them */
    mpf_clear(Qf);
    mpf_clear(Tf);
    mpf_clear(sqrtC);
    mpf_clear(num);
    operands_live = 0;

    if (cancelled(progress)) goto done;
    /* Straight into the caller's buffer: ask for digits+1 significant digits
     * (the leading "3" plus the rest) at buf+1, which leaves exactly the room
     * needed to shift the "3" down and drop a "." in behind it. */
    mpf_get_str(buf + 1, &exp, 10, (size_t)digits + 1, pi);
    if (buf[1] == '\0') goto done;

    /* mpf_get_str strips trailing zeros, so it can legitimately hand back
     * fewer characters than were asked for - roughly one digit count in ten.
     * Pad the shortfall instead of leaving the tail of the buffer unwritten. */
    got = strlen(buf + 1);
    if (got < (size_t)digits + 1)
        memset(buf + 1 + got, '0', (size_t)digits + 1 - got);
    buf[0] = buf[1];
    buf[1] = '.';
    buf[digits + 2] = '\0';
    rc = 0;

done:
    if (operands_live) {
        mpf_clear(Qf);
        mpf_clear(Tf);
        mpf_clear(sqrtC);
        mpf_clear(num);
    }
    mpf_clear(pi);
    return rc;
}

char *pi_finalize(long digits, const pi_bs_t *r, pi_progress_t *progress) {
    char *out = (char *)malloc(pi_result_size(digits));
    if (!out) {   /* at these sizes the allocation really can fail */
        pi_bs_clear((pi_bs_t *)r);
        return NULL;
    }
    if (pi_finalize_into(digits, r, progress, out) != 0) {
        free(out);
        return NULL;
    }
    return out;
}

void pi_free(char *s) { free(s); }

int pi_compute(long digits, pi_progress_t *progress, char **out) {
    if (digits < 1 || digits > pi_max_digits()) return -1;

    long terms = pi_terms_for_digits(digits);
    if (progress) {
        progress->leaves_total = terms;
        progress->leaves_done = 0;
    }

    pi_bs_t r;
    pi_bs_init(&r);
    pi_bs_compute_root(0, terms, &r, progress);

    if (cancelled(progress)) {
        pi_bs_clear(&r);
        return -2;
    }

    *out = pi_finalize(digits, &r, progress);  /* consumes r */
    if (!*out) return -2;
    return 0;
}

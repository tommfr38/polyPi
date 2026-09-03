#include "pi_engine.h"
#include <stdlib.h>
#include <string.h>

#define CHUD_A 13591409UL
#define CHUD_B 545140134UL
#define CHUD_C 640320UL

/* digits per Chudnovsky term: log10(640320^3 / 24) / 2 ~= 14.1816 */
long pi_terms_for_digits(long digits) {
    long terms = (long)(digits / 14.1816) + 2;
    return terms < 1 ? 1 : terms;
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

static void c3_over_24(mpz_t out) {
    mpz_init_set_ui(out, CHUD_C);
    mpz_pow_ui(out, out, 3);
    mpz_divexact_ui(out, out, 24);
}

void pi_bs_combine(const pi_bs_t *left, const pi_bs_t *right, pi_bs_t *out) {
    mpz_t t1, t2;
    mpz_init(t1);
    mpz_init(t2);
    mpz_mul(out->P, left->P, right->P);
    mpz_mul(out->Q, left->Q, right->Q);
    mpz_mul(t1, left->T, right->Q);
    mpz_mul(t2, left->P, right->T);
    mpz_add(out->T, t1, t2);
    mpz_clear(t1);
    mpz_clear(t2);
}

static int cancelled(const pi_progress_t *progress) {
    return progress && PI_LOAD(&progress->cancel);
}

static void set_identity(pi_bs_t *out) {
    mpz_set_ui(out->P, 1);
    mpz_set_ui(out->Q, 1);
    mpz_set_ui(out->T, 0);
}

void pi_bs_compute(long a, long b, pi_bs_t *out, pi_progress_t *progress) {
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

    long m = a + (b - a) / 2;
    pi_bs_t left, right;
    pi_bs_init(&left);
    pi_bs_init(&right);
    pi_bs_compute(a, m, &left, progress);
    pi_bs_compute(m, b, &right, progress);
    /* The merges are where the huge multiplications live, and they happen as
     * the recursion unwinds. Without this check a cancel still has to grind
     * through one full-size mpz_mul per stack level before it takes effect. */
    if (cancelled(progress)) {
        set_identity(out);
    } else {
        pi_bs_combine(&left, &right, out);
    }
    pi_bs_clear(&left);
    pi_bs_clear(&right);
}

char *pi_finalize(long digits, const pi_bs_t *r, pi_progress_t *progress) {
    if (progress) PI_STORE(&progress->phase, PI_PHASE_FINALIZING);

    mp_bitcnt_t prec = (mp_bitcnt_t)(digits * 3.3219281) + 256;
    mpf_set_default_prec(prec);

    mpf_t Qf, Tf, sqrtC, num, pi;
    mp_exp_t exp;
    char *raw = NULL;
    char *out = NULL;

    mpf_init(Qf);
    mpf_init(Tf);
    mpf_init(sqrtC);
    mpf_init(num);
    mpf_init(pi);

    mpf_set_z(Qf, r->Q);
    mpf_set_z(Tf, r->T);

    /* Each of the steps below is a single opaque GMP call, so the cancel flag
     * can only be honoured between them - the string conversion in particular
     * is the largest uninterruptible chunk of a run. */
    if (cancelled(progress)) goto done;
    mpf_sqrt_ui(sqrtC, 10005);
    mpf_mul(num, Qf, sqrtC);
    mpf_mul_ui(num, num, 426880);

    if (cancelled(progress)) goto done;
    mpf_div(pi, num, Tf);

    if (cancelled(progress)) goto done;
    /* ask for digits+1 significant digits (the leading "3" plus the rest) */
    raw = mpf_get_str(NULL, &exp, 10, digits + 1, pi);
    if (!raw || raw[0] == '\0') goto done;

    {
        /* mpf_get_str strips trailing zeros, so it can legitimately hand back
         * fewer characters than were asked for - roughly one digit count in
         * ten. Copying a fixed `digits` bytes then reads past its buffer,
         * which traps under wasm and silently yields a junk digit natively. */
        size_t got = strlen(raw);
        size_t after_point = got - 1;
        size_t copy = after_point < (size_t)digits ? after_point : (size_t)digits;

        out = (char *)malloc((size_t)digits + 3);
        if (!out) goto done;   /* at these sizes the allocation really can fail */
        out[0] = raw[0];
        out[1] = '.';
        memcpy(out + 2, raw + 1, copy);
        memset(out + 2 + copy, '0', (size_t)digits - copy);
        out[digits + 2] = '\0';
    }

done:
    free(raw);
    mpf_clear(Qf);
    mpf_clear(Tf);
    mpf_clear(sqrtC);
    mpf_clear(num);
    mpf_clear(pi);

    return out;
}

void pi_free(char *s) { free(s); }

int pi_compute(long digits, pi_progress_t *progress, char **out) {
    if (digits < 1) return -1;

    long terms = pi_terms_for_digits(digits);
    if (progress) {
        progress->leaves_total = terms;
        progress->leaves_done = 0;
    }

    pi_bs_t r;
    pi_bs_init(&r);
    pi_bs_compute(0, terms, &r, progress);

    if (cancelled(progress)) {
        pi_bs_clear(&r);
        return -2;
    }

    *out = pi_finalize(digits, &r, progress);
    pi_bs_clear(&r);
    if (!*out) return -2;
    return 0;
}

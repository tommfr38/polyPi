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

void pi_bs_compute(long a, long b, pi_bs_t *out, pi_progress_t *progress) {
    if (progress && progress->cancel) {
        mpz_set_ui(out->P, 1);
        mpz_set_ui(out->Q, 1);
        mpz_set_ui(out->T, 0);
        return;
    }

    if (b - a == 1) {
        static __thread int have_c3 = 0;
        static __thread mpz_t c3;
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

        if (progress) __atomic_add_fetch(&progress->leaves_done, 1, __ATOMIC_RELAXED);
        return;
    }

    long m = a + (b - a) / 2;
    pi_bs_t left, right;
    pi_bs_init(&left);
    pi_bs_init(&right);
    pi_bs_compute(a, m, &left, progress);
    pi_bs_compute(m, b, &right, progress);
    pi_bs_combine(&left, &right, out);
    pi_bs_clear(&left);
    pi_bs_clear(&right);
}

char *pi_finalize(long digits, const pi_bs_t *r) {
    mp_bitcnt_t prec = (mp_bitcnt_t)(digits * 3.3219281) + 256;
    mpf_set_default_prec(prec);

    mpf_t Qf, Tf, sqrtC, num, pi;
    mpf_init(Qf);
    mpf_init(Tf);
    mpf_init(sqrtC);
    mpf_init(num);
    mpf_init(pi);

    mpf_set_z(Qf, r->Q);
    mpf_set_z(Tf, r->T);

    mpf_sqrt_ui(sqrtC, 10005);
    mpf_mul(num, Qf, sqrtC);
    mpf_mul_ui(num, num, 426880);

    mpf_div(pi, num, Tf);

    /* format with gmp: get exactly digits+1 significant digits (leading "3") */
    mp_exp_t exp;
    char *raw = mpf_get_str(NULL, &exp, 10, digits + 1, pi);

    char *out = (char *)malloc(digits + 3);
    out[0] = raw[0];
    out[1] = '.';
    memcpy(out + 2, raw + 1, digits);
    out[digits + 2] = '\0';

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

    if (progress && progress->cancel) {
        pi_bs_clear(&r);
        return -2;
    }

    *out = pi_finalize(digits, &r);
    pi_bs_clear(&r);
    return 0;
}

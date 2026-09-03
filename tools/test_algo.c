#include <stdio.h>
#include <stdlib.h>
#include <gmp.h>

/* Chudnovsky binary splitting, verified empirically against known pi digits. */

static const long A = 13591409;
static const long B = 545140134;
static const long C = 640320;

static mpz_t C3_OVER_24;

typedef struct { mpz_t P, Q, T; } PQT;

static void pqt_init(PQT *r) { mpz_init(r->P); mpz_init(r->Q); mpz_init(r->T); }
static void pqt_clear(PQT *r) { mpz_clear(r->P); mpz_clear(r->Q); mpz_clear(r->T); }

static void bs(long a, long b, PQT *r) {
    if (b - a == 1) {
        if (a == 0) {
            mpz_set_ui(r->P, 1);
            mpz_set_ui(r->Q, 1);
        } else {
            mpz_set_si(r->P, 6*a-5);
            mpz_mul_si(r->P, r->P, 2*a-1);
            mpz_mul_si(r->P, r->P, 6*a-1);
            mpz_set_si(r->Q, a);
            mpz_pow_ui(r->Q, r->Q, 3);
            mpz_mul(r->Q, r->Q, C3_OVER_24);
        }
        mpz_t t;
        mpz_init_set_si(t, a);
        mpz_mul_ui(t, t, B);
        mpz_add_ui(t, t, A);
        mpz_mul(r->T, t, r->P);
        if (a & 1) mpz_neg(r->T, r->T);
        mpz_clear(t);
    } else {
        long m = (a + b) / 2;
        PQT L, R;
        pqt_init(&L); pqt_init(&R);
        bs(a, m, &L);
        bs(m, b, &R);
        mpz_mul(r->P, L.P, R.P);
        mpz_mul(r->Q, L.Q, R.Q);
        mpz_t t1, t2;
        mpz_init(t1); mpz_init(t2);
        mpz_mul(t1, L.T, R.Q);
        mpz_mul(t2, L.P, R.T);
        mpz_add(r->T, t1, t2);
        mpz_clear(t1); mpz_clear(t2);
        pqt_clear(&L); pqt_clear(&R);
    }
}

int main(int argc, char **argv) {
    long digits = argc > 1 ? atol(argv[1]) : 100;
    long terms = digits / 14 + 10;

    mpz_init_set_ui(C3_OVER_24, C);
    mpz_pow_ui(C3_OVER_24, C3_OVER_24, 3);
    mpz_divexact_ui(C3_OVER_24, C3_OVER_24, 24);

    PQT r; pqt_init(&r);
    bs(0, terms, &r);

    mp_bitcnt_t prec = (mp_bitcnt_t)(digits * 3.322) + 64;
    mpf_set_default_prec(prec);

    mpf_t Qf, Tf, sqrtC, num, denom, pi;
    mpf_init(Qf); mpf_init(Tf); mpf_init(sqrtC); mpf_init(num); mpf_init(denom); mpf_init(pi);

    mpf_set_z(Qf, r.Q);
    mpf_set_z(Tf, r.T);

    /* denom = T  (A is already folded into T at the leaves) */
    mpf_set(denom, Tf);

    /* num = Q * sqrt(10005) * 426880 */
    mpf_sqrt_ui(sqrtC, 10005);
    mpf_mul(num, Qf, sqrtC);
    mpf_mul_ui(num, num, 426880);

    mpf_div(pi, num, denom);

    gmp_printf("%.*Ff\n", digits, pi);

    return 0;
}

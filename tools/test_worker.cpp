/* Self-test for the pi engine and the threaded worker.
 *
 * Links the real core and the real PiWorker (which needs no UI), so the
 * chunk-and-merge path that only the desktop app exercises is actually run
 * rather than merely compiled. Build with -DPOLYPI_BUILD_TESTS=ON.
 */
#include "pi_worker.h"

#include <gmp.h>

#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <string>
#include <thread>

/* First 1100 digits after the point, cross-checked against Machin's formula. */
static const char *kPiDigits =
    "1415926535897932384626433832795028841971693993751058209749445923078164"
    "0628620899862803482534211706798214808651328230664709384460955058223172"
    "5359408128481117450284102701938521105559644622948954930381964428810975"
    "6659334461284756482337867831652712019091456485669234603486104543266482"
    "1339360726024914127372458700660631558817488152092096282925409171536436"
    "7892590360011330530548820466521384146951941511609433057270365759591953"
    "0921861173819326117931051185480744623799627495673518857527248912279381"
    "8301194912983367336244065664308602139494639522473719070217986094370277"
    "0539217176293176752384674818467669405132000568127145263560827785771342"
    "7577896091736371787214684409012249534301465495853710507922796892589235"
    "4201995611212902196086403441815981362977477130996051870721134999999837"
    "2978049951059731732816096318595024459455346908302642522308253344685035"
    "2619311881710100031378387528865875332083814206171776691473035982534904"
    "2875546873115956286388235378759375195778185778053217122680661300192787"
    "6611195909216420198938095257201065485863278865936153381827968230301952"
    "03530185296899577362259941389124972177528347913151";

static int g_failures = 0;

static void fail(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    std::fputs("FAIL: ", stderr);
    std::vfprintf(stderr, fmt, ap);
    std::fputc('\n', stderr);
    va_end(ap);
    g_failures++;
}

/* Runs one computation to completion. Returns false if it errored out. */
static bool computeSync(long digits, int threads, std::string &out) {
    PiWorker w;
    w.start(digits, threads);
    while (w.isRunning()) std::this_thread::sleep_for(std::chrono::milliseconds(2));
    if (w.hasError()) return false;
    if (!w.isDone()) return false;
    out = w.takeResult();
    return true;
}

static void checkAgainstReference(long digits, int threads) {
    std::string got;
    if (!computeSync(digits, threads, got)) {
        fail("%ld digits on %d threads did not complete", digits, threads);
        return;
    }
    std::string want = std::string("3.") + std::string(kPiDigits, (size_t)digits);
    if (got.size() != (size_t)digits + 2)
        fail("%ld digits on %d threads: got %zu chars, want %ld",
             digits, threads, got.size(), digits + 2);
    if (got != want) {
        size_t i = 0;
        while (i < got.size() && i < want.size() && got[i] == want[i]) i++;
        fail("%ld digits on %d threads: first wrong character at index %zu (got '%c')",
             digits, threads, i, i < got.size() ? got[i] : '?');
    }
}

/* Every thread count has to produce byte-identical output: the chunking and
 * the merge tree must not change the answer. */
static void checkThreadsAgree(long digits) {
    static const int kThreadCounts[] = {1, 2, 3, 4, 5, 7, 8, 13, 16};
    std::string base;
    if (!computeSync(digits, 1, base)) {
        fail("%ld digits single-threaded did not complete", digits);
        return;
    }
    for (int t : kThreadCounts) {
        std::string got;
        if (!computeSync(digits, t, got)) {
            fail("%ld digits on %d threads did not complete", digits, t);
            continue;
        }
        if (got != base)
            fail("%ld digits: %d threads disagrees with 1 thread", digits, t);
    }
}

static void checkCancelStops() {
    PiWorker w;
    w.start(20000000, 4);
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
    w.cancel();
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(60);
    while (w.isRunning() && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    if (w.isRunning()) { fail("cancel did not stop the run within 60s"); return; }
    if (!w.wasCancelled()) fail("cancelled run did not report itself cancelled");
    if (w.isDone()) fail("cancelled run reported a result");
}

static void checkDigitCap() {
    long cap = PiWorker::maxDigits();
    if (cap < 1000000000L)
        fail("digit cap is %ld, below the 1B the UI offers", cap);
    /* On a 32-bit mp_bitcnt_t build the cap must land below where the working
     * precision would wrap; on a 64-bit one it is bounded by long instead. */
    if (sizeof(mp_bitcnt_t) == 4 && cap > 1300000000L)
        fail("digit cap %ld is past where the precision wraps", cap);
}

int main(void) {
    /* Sizes chosen so the first digit dropped is 4 or less: whether the
     * formatting step rounds the last digit or truncates it, the expected
     * output is the same, and the check stays about the digits themselves. */
    static const long kSizes[] = {1, 2, 5, 14, 101, 502, 1000, 1091};
    for (long d : kSizes) checkAgainstReference(d, 1);
    for (long d : kSizes) checkAgainstReference(d, 8);
    checkAgainstReference(1091, 3);

    checkThreadsAgree(1000);
    checkThreadsAgree(10000);
    checkThreadsAgree(50000);

    checkDigitCap();
    checkCancelStops();

    if (g_failures) {
        std::fprintf(stderr, "%d check(s) failed\n", g_failures);
        return 1;
    }
    std::puts("all checks passed");
    return 0;
}

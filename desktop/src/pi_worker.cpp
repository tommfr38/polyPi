#include "pi_worker.h"
#include "pi_engine.h"
#include <atomic>
#include <cstdlib>
#include <new>
#include <vector>
#include <algorithm>

struct PiWorker::ProgressState {
    pi_progress_t p{0, 0, 0, 0};
};

/* GMP's default reaction to a failed allocation is to print to stderr and
 * abort(), which on a windowed build means the app vanishes with no message -
 * and it is unreachable by the try/catch below, because abort() never unwinds.
 * These replacements throw instead, so a run that is too big for the machine
 * comes back as an error the UI can explain. GMP calls them through C function
 * pointers, hence the linkage. */
extern "C" {

static void *piGmpAlloc(size_t n) {
    void *p = std::malloc(n ? n : 1);
    if (!p) throw std::bad_alloc();
    return p;
}

static void *piGmpRealloc(void *p, size_t, size_t n) {
    void *q = std::realloc(p, n ? n : 1);
    if (!q) throw std::bad_alloc();
    return q;
}

static void piGmpFree(void *p, size_t) { std::free(p); }

}  // extern "C"

namespace {

/* Must happen before the first GMP object exists, so that every block GMP
 * ever frees was allocated by the same functions. */
void installGmpAllocators() {
    static std::once_flag once;
    std::call_once(once, [] {
        mp_set_memory_functions(piGmpAlloc, piGmpRealloc, piGmpFree);
    });
}

}  // namespace

long PiWorker::maxDigits() { return pi_max_digits(); }

PiWorker::~PiWorker() {
    cancel();
    if (thread_.joinable()) thread_.join();
    delete progress_;
}

void PiWorker::cancel() {
    if (progress_) PI_STORE(&progress_->p.cancel, 1);
}

void PiWorker::start(long digits, int threads) {
    if (running_.load()) return;
    if (thread_.joinable()) thread_.join();
    installGmpAllocators();
    delete progress_;
    progress_ = new ProgressState();

    threads = std::max(1, threads);
    lastDigits_ = digits;
    lastThreads_ = threads;
    done_ = false;
    error_ = false;
    cancelled_ = false;
    running_ = true;
    startTime_ = std::chrono::steady_clock::now();

    thread_ = std::thread(&PiWorker::run, this, digits, threads);
}

void PiWorker::run(long digits, int threads) {
    try {
        runInner(digits, threads);
    } catch (...) {
        // out of memory is the realistic case here; report it instead of
        // letting the exception escape the thread and abort the process
        error_ = true;
        running_ = false;
        endTime_ = std::chrono::steady_clock::now();
    }
}

void PiWorker::runInner(long digits, int threads) {
    long terms = pi_terms_for_digits(digits);
    progress_->p.leaves_total = terms;
    progress_->p.leaves_done = 0;

    int nchunks = (int)std::min<long>(threads, std::max<long>(1, terms));
    std::vector<pi_bs_t> chunks(nchunks);
    std::vector<std::thread> workers;

    long chunkSize = terms / nchunks;
    long remainder = terms % nchunks;
    std::vector<std::pair<long, long>> ranges(nchunks);
    long cursor = 0;
    for (int i = 0; i < nchunks; i++) {
        long size = chunkSize + (i < remainder ? 1 : 0);
        ranges[i] = {cursor, cursor + size};
        cursor += size;
    }

    for (int i = 0; i < nchunks; i++) {
        pi_bs_init(&chunks[i]);
    }

    std::atomic<bool> failed{false};
    // One thread running out of memory dooms the whole run, so stop the
    // others rather than leaving them to chase the same allocation. An
    // exception must not escape a std::thread: that is a straight terminate().
    auto fail = [this, &failed]() {
        failed.store(true);
        PI_STORE(&progress_->p.cancel, 1);
    };
    auto abandon = [&chunks]() {
        for (auto &c : chunks) pi_bs_clear(&c);
    };
    auto finish = [this](bool wasCancel) {
        endTime_ = std::chrono::steady_clock::now();
        if (wasCancel) cancelled_ = true; else error_ = true;
        running_ = false;
    };

    for (int i = 0; i < nchunks; i++) {
        workers.emplace_back([this, i, nchunks, &chunks, &ranges, &fail]() {
            try {
                // with a single chunk there is no merge, so its P is dead
                if (nchunks == 1)
                    pi_bs_compute_root(ranges[i].first, ranges[i].second, &chunks[i], &progress_->p);
                else
                    pi_bs_compute(ranges[i].first, ranges[i].second, &chunks[i], &progress_->p);
            } catch (...) {
                fail();
            }
        });
    }
    for (auto &t : workers) t.join();

    if (failed.load()) { abandon(); finish(false); return; }
    if (PI_LOAD(&progress_->p.cancel)) { abandon(); finish(true); return; }

    // Merge the chunks pairwise up a balanced tree, each round in parallel.
    //
    // The obvious left-fold - accumulate chunk 1, then 2, then 3 - makes every
    // one of the n-1 merges multiply against a nearly full-size accumulator,
    // on one thread. That serial tail grew with the thread count as fast as
    // the parallel counting shrank, which is why raising the slider stopped
    // making large runs finish any sooner. A balanced tree does the same work
    // in log2(n) rounds, and every round but the last is parallel.
    PI_STORE(&progress_->p.phase, PI_PHASE_MERGING);
    for (int step = 1; step < nchunks; step *= 2) {
        std::vector<std::thread> round;
        for (int i = 0; i + step < nchunks; i += 2 * step) {
            // nothing gets merged onto the result of the very last merge, so
            // its P - 700 MB and a full-size multiplication - is not built
            bool last = (i == 0 && step * 2 >= nchunks);
            round.emplace_back([&chunks, i, step, last, &fail]() {
                try {
                    pi_bs_merge(&chunks[i], &chunks[i + step], last ? 0 : 1);
                } catch (...) {
                    fail();
                }
            });
        }
        for (auto &t : round) t.join();
        if (PI_LOAD(&progress_->p.cancel)) break;
    }

    if (failed.load()) { abandon(); finish(false); return; }
    if (PI_LOAD(&progress_->p.cancel)) { abandon(); finish(true); return; }

    // everything is folded into chunks[0]; the rest are empty but still owned
    for (int i = 1; i < nchunks; i++) pi_bs_clear(&chunks[i]);

    // Format straight into the string that will hold the result. Going via a
    // separate buffer and copying would mean two gigabyte-scale allocations
    // live at once, at the very end of a run that is already short on room.
    std::string text;
    try {
        text.resize(pi_result_size(digits));
    } catch (...) {
        pi_bs_clear(&chunks[0]);   // not handed over yet; don't leak it
        throw;
    }
    int rc = pi_finalize_into(digits, &chunks[0], &progress_->p, &text[0]);  // consumes chunks[0]
    endTime_ = std::chrono::steady_clock::now();

    if (rc != 0) {
        if (PI_LOAD(&progress_->p.cancel)) cancelled_ = true; else error_ = true;
        running_ = false;
        return;
    }
    text.resize((size_t)digits + 2);   // drop the terminator slot

    {
        std::lock_guard<std::mutex> lock(resultMutex_);
        result_ = std::move(text);
    }

    // done_ before running_: a poller that sees the run stop must already be
    // able to see the result, or it reads back an empty string
    done_ = true;
    running_ = false;
}

bool PiWorker::isMerging() const {
    if (!progress_) return false;
    return PI_LOAD(&progress_->p.phase) == PI_PHASE_MERGING;
}

bool PiWorker::isFinalizing() const {
    if (!progress_) return false;
    return PI_LOAD(&progress_->p.phase) == PI_PHASE_FINALIZING;
}

double PiWorker::progress() const {
    if (!progress_ || progress_->p.leaves_total <= 0) return 0.0;
    return (double)progress_->p.leaves_done / (double)progress_->p.leaves_total;
}

long PiWorker::elapsedMs() const {
    auto end = done_.load() ? endTime_ : std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(end - startTime_).count();
}

long PiWorker::digitsPerSecEstimate() const {
    long ms = elapsedMs();
    if (ms <= 0) return 0;
    double p = progress();
    if (p <= 0) return 0;
    double estDigitsDone = p * (double)lastDigits_;
    return (long)(estDigitsDone / (ms / 1000.0));
}

std::string PiWorker::takeResult() {
    std::lock_guard<std::mutex> lock(resultMutex_);
    return std::move(result_);
}

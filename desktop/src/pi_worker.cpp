#include "pi_worker.h"
#include "pi_engine.h"
#include <vector>
#include <algorithm>

struct PiWorker::ProgressState {
    pi_progress_t p{0, 0, 0};
};

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
    } catch (const std::exception &) {
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

    for (int i = 0; i < nchunks; i++) {
        workers.emplace_back([this, i, &chunks, &ranges]() {
            pi_bs_compute(ranges[i].first, ranges[i].second, &chunks[i], &progress_->p);
        });
    }
    for (auto &t : workers) t.join();

    if (PI_LOAD(&progress_->p.cancel)) {
        for (auto &c : chunks) pi_bs_clear(&c);
        endTime_ = std::chrono::steady_clock::now();
        cancelled_ = true;
        running_ = false;
        return;
    }

    // left-fold combine chunks[0..n) into acc
    pi_bs_t acc;
    pi_bs_init(&acc);
    mpz_set(acc.P, chunks[0].P);
    mpz_set(acc.Q, chunks[0].Q);
    mpz_set(acc.T, chunks[0].T);
    for (int i = 1; i < nchunks; i++) {
        pi_bs_t next;
        pi_bs_init(&next);
        pi_bs_combine(&acc, &chunks[i], &next);
        pi_bs_clear(&acc);
        acc = next;
    }

    for (auto &c : chunks) pi_bs_clear(&c);   /* before finalize, not after */
    char *out = pi_finalize(digits, &acc, &progress_->p);  /* consumes acc */
    endTime_ = std::chrono::steady_clock::now();

    if (!out) { // cancelled, or the final buffer couldn't be allocated
        if (PI_LOAD(&progress_->p.cancel)) cancelled_ = true; else error_ = true;
        running_ = false;
        return;
    }

    {
        std::lock_guard<std::mutex> lock(resultMutex_);
        result_ = out;
    }
    pi_free(out);

    running_ = false;
    done_ = true;
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

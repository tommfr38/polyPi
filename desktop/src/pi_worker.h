#pragma once
#include <atomic>
#include <string>
#include <thread>
#include <chrono>
#include <mutex>

class PiWorker {
public:
    // Largest digit count this build can represent - see pi_max_digits().
    // Callers must clamp to it: past this the run returns nonsense.
    static long maxDigits();

    ~PiWorker();

    void start(long digits, int threads);
    void cancel();

    bool isRunning() const { return running_.load(); }
    bool isDone() const { return done_.load(); }
    bool hasError() const { return error_.load(); }
    bool wasCancelled() const { return cancelled_.load(); }

    // true while the per-thread partial results are being merged: the leaf
    // counter has already hit 100%, so without this the run looks stalled
    bool isMerging() const;

    // true once the run has moved past merging into the (largely
    // uninterruptible) formatting stage
    bool isFinalizing() const;

    double progress() const;
    long elapsedMs() const;
    long digitsPerSecEstimate() const;

    // valid once isDone(); clears internal storage on call
    std::string takeResult();

    long lastDigits() const { return lastDigits_; }
    int lastThreads() const { return lastThreads_; }

private:
    void run(long digits, int threads);
    void runInner(long digits, int threads);

    std::thread thread_;
    struct ProgressState; // wraps pi_progress_t, defined in .cpp to avoid leaking gmp.h here
    ProgressState *progress_ = nullptr;

    std::atomic<bool> running_{false};
    std::atomic<bool> done_{false};
    std::atomic<bool> error_{false};
    std::atomic<bool> cancelled_{false};

    std::chrono::steady_clock::time_point startTime_;
    std::chrono::steady_clock::time_point endTime_;

    std::mutex resultMutex_;
    std::string result_;

    long lastDigits_ = 0;
    int lastThreads_ = 1;
};

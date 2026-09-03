#pragma once
#include <atomic>
#include <string>
#include <thread>
#include <chrono>
#include <mutex>

class PiWorker {
public:
    ~PiWorker();

    void start(long digits, int threads);
    void cancel();

    bool isRunning() const { return running_.load(); }
    bool isDone() const { return done_.load(); }
    bool hasError() const { return error_.load(); }

    double progress() const;
    long elapsedMs() const;
    long digitsPerSecEstimate() const;

    // valid once isDone(); clears internal storage on call
    std::string takeResult();

    long lastDigits() const { return lastDigits_; }
    int lastThreads() const { return lastThreads_; }

private:
    void run(long digits, int threads);

    std::thread thread_;
    struct ProgressState; // wraps pi_progress_t, defined in .cpp to avoid leaking gmp.h here
    ProgressState *progress_ = nullptr;

    std::atomic<bool> running_{false};
    std::atomic<bool> done_{false};
    std::atomic<bool> error_{false};

    std::chrono::steady_clock::time_point startTime_;
    std::chrono::steady_clock::time_point endTime_;

    std::mutex resultMutex_;
    std::string result_;

    long lastDigits_ = 0;
    int lastThreads_ = 1;
};

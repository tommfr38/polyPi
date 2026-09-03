#pragma once
#include <atomic>
#include <mutex>
#include <string>
#include <thread>

// Checks GitHub Releases for a newer tag than POLYPI_VERSION. Runs the HTTP
// request on a background thread so it never blocks the UI (matters most on
// the automatic startup check).
class Updater {
public:
    ~Updater();

    void checkAsync();

    bool isChecking() const { return checking_.load(); }
    bool hasChecked() const { return checked_.load(); }
    bool hasError() const { return error_.load(); }
    bool updateAvailable() const { return updateAvailable_.load(); }

    std::string latestVersion() const;
    std::string releaseUrl() const;

private:
    void run();

    std::thread thread_;
    std::atomic<bool> checking_{false};
    std::atomic<bool> checked_{false};
    std::atomic<bool> error_{false};
    std::atomic<bool> updateAvailable_{false};

    mutable std::mutex mutex_;
    std::string latestVersion_;
    std::string releaseUrl_;
};

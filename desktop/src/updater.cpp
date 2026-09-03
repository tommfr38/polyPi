#include "updater.h"
#include "config.h"
#include <curl/curl.h>
#include <cstring>

static size_t writeCallback(char *ptr, size_t size, size_t nmemb, void *userdata) {
    auto *out = static_cast<std::string *>(userdata);
    out->append(ptr, size * nmemb);
    return size * nmemb;
}

// Minimal extraction of a top-level "key":"value" string field from a small
// JSON payload. GitHub's release API response is well-formed and flat enough
// that this avoids pulling in a full JSON dependency for one field lookup.
static std::string extractJsonString(const std::string &json, const std::string &key) {
    std::string needle = "\"" + key + "\"";
    size_t pos = json.find(needle);
    if (pos == std::string::npos) return "";
    pos = json.find(':', pos + needle.size());
    if (pos == std::string::npos) return "";
    pos = json.find('"', pos);
    if (pos == std::string::npos) return "";
    size_t end = pos + 1;
    std::string value;
    while (end < json.size() && json[end] != '"') {
        if (json[end] == '\\' && end + 1 < json.size()) end++;
        value += json[end];
        end++;
    }
    return value;
}

static bool versionLess(const std::string &a, const std::string &b) {
    auto parse = [](const std::string &v) {
        std::string s = v;
        if (!s.empty() && s[0] == 'v') s = s.substr(1);
        int parts[3] = {0, 0, 0};
        sscanf(s.c_str(), "%d.%d.%d", &parts[0], &parts[1], &parts[2]);
        return parts[0] * 1000000 + parts[1] * 1000 + parts[2];
    };
    return parse(a) < parse(b);
}

Updater::~Updater() {
    if (thread_.joinable()) thread_.join();
}

void Updater::checkAsync() {
    if (checking_.load()) return;
    if (thread_.joinable()) thread_.join();
    checking_ = true;
    checked_ = false;
    error_ = false;
    thread_ = std::thread(&Updater::run, this);
}

void Updater::run() {
    CURL *curl = curl_easy_init();
    if (!curl) {
        error_ = true;
        checking_ = false;
        checked_ = true;
        return;
    }

    std::string url = std::string("https://api.github.com/repos/") + POLYPI_GITHUB_REPO + "/releases/latest";
    std::string response;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "polyPi-updater/" POLYPI_VERSION);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 8L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    CURLcode res = curl_easy_perform(curl);
    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK || httpCode != 200) {
        error_ = true;
        checking_ = false;
        checked_ = true;
        return;
    }

    std::string tag = extractJsonString(response, "tag_name");
    std::string url_ = extractJsonString(response, "html_url");

    {
        std::lock_guard<std::mutex> lock(mutex_);
        latestVersion_ = tag;
        releaseUrl_ = url_;
    }

    if (!tag.empty()) {
        updateAvailable_ = versionLess(POLYPI_VERSION, tag);
    }

    checking_ = false;
    checked_ = true;
}

std::string Updater::latestVersion() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return latestVersion_;
}

std::string Updater::releaseUrl() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return releaseUrl_;
}

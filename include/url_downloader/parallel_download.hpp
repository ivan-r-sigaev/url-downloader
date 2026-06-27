#pragma once
#include <curl_easy.h>
#include <filesystem>
#include <optional>

// Internal structure used by ParallelDownload to manage a single download connection.
class DownloadInstance {
public:
    // Easy handle associated with the download.
    curl::curl_easy easy_handle;
    // Url to download from.
    std::string url;
    // Path to the file that is being downloaded.
    std::filesystem::path output_file_path;
    // The file that is being downloaded.
    std::ofstream output_file;
    // Exact time point when the download started.
    std::optional<std::chrono::steady_clock::time_point> start_time;
public:
    // Explicit constructor.
    explicit DownloadInstance(
        curl::curl_easy _easy_handle,
        std::string _url,
        std::filesystem::path _output_file_path,
        std::ofstream _output_file,
        std::optional<std::chrono::steady_clock::time_point> _start_time
    );
    DownloadInstance(const DownloadInstance&) = delete;
    DownloadInstance& operator=(const DownloadInstance&) = delete;
    DownloadInstance(DownloadInstance&&) = default;
    DownloadInstance& operator=(DownloadInstance&&) = default;
};

// Simplifies downloading multiple urls at once.
class ParallelDownload {
public:
    ParallelDownload() = default;
    ParallelDownload(const ParallelDownload&) = delete;
    ParallelDownload& operator=(const ParallelDownload&) = delete;
    // Enqueue a new url to be downloaded.
    // Does not start downloading until the `.perform()` is called.
    void add(const std::string& url, const std::filesystem::path& output_directory);
    // Begins downloading all of the enqueued urls.
    void perform(long max_parallel_downloads);
private:
    std::vector<std::optional<DownloadInstance>> downloads {};
private:
    static size_t header_callback(void *buffer, size_t size, size_t nitems, void *userdata);
    static size_t write_callback(void *ptr, size_t size, size_t nmemb, void *userdata);
};
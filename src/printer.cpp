#include "url_downloader/printer.hpp"
#include <iostream>
#include <sstream>

// Returns current time formated as "YYYY-MM-DD hh:mm:ss.sss".
static std::string current_time_to_string() {
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    
    auto tt = std::chrono::system_clock::to_time_t(now);
    auto tm = std::localtime(&tt);

    auto oss = std::ostringstream();
    oss << std::put_time(tm, "%F %H:%M:%S")
        << '.' << std::setfill('0') << std::setw(3) << ms.count();
    
    return oss.str();
}

// Normal output stream.
static std::ostream& out() {
    return std::cout;
}

// Error output stream.
static std::ostream& err() {
    return std::cerr;
}

void print_app_start() {
    out() << current_time_to_string() << " Url Downloader Started" << std::endl;
}

void print_app_arguments(Arguments args) {
    out()
        << current_time_to_string()
        << " Arguments: { "
        << " urls-file-path: `" << args.urls_file << '`'
        << " ouput-direcotry-path: `" << args.output_directory << '`'
        << " max-parallel-downloads: " << args.max_parallel_downloads
        << " }" << std::endl;
}

void print_app_usage(std::string command_name) {
    err()
        << current_time_to_string() 
        << " Usage: " << command_name << " <urls_file> <out_dir> <parallel_download_count>"
        << std::endl;
}

void print_unable_to_create_output(const std::filesystem::path& output_directory) {
    err()
        << current_time_to_string()
        << " Error: \"Unable to create output directory\""
        << " Path: `" << output_directory << '`'
        << std::endl;
}

void print_download_error(const std::string& url, long http_code) {
    err()
        << current_time_to_string()
        << " Error: \"Failed to obtain URL contents\""
        << " Code: " << http_code 
        << " Url: `" << url << "`"
        << url 
        << std::endl;
}

void print_download_success(
    const std::string& url,
    const std::filesystem::path& output_file_path,
    std::chrono::milliseconds elapsed
) {
    out()
        << current_time_to_string()
        << " " << url
        << " - finished downloading in " << elapsed.count() << " ms"
        << " [" << output_file_path << "]"
        << std::endl;
}

void print_download_start(const std::string& url) {
    out()
        << current_time_to_string()
        << " " << url
        << " - started downloading"
        << std::endl;
}

void print_url_skipped(const std::string& url) {
    err()
        << current_time_to_string()
        << " Error: \"URL skipped: unknown protocol, must be HTTP/HTTPS\""
        << " Url: `" << url << '`'
        << std::endl;
}

void print_libcurl_crash() {
    err() 
        << current_time_to_string()
        << " Error: \"internal libcurl error, crashing\""
        << std::endl;
}

void print_app_finish() {
    out() 
        << current_time_to_string()
        << " Url Downloader Finished"
        << std::endl;
}

void print_callback_exception(std::exception e) {
    err()
        << current_time_to_string()
        << " Error: \"A exception occured in libcurl callback function\""
        << " What: `" << e.what() << "`"
        << std::endl;
}

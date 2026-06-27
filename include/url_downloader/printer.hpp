#pragma once
#include "arguments.hpp"
#include <ostream>
#include <chrono>
#include <filesystem>

// Keeps all printing-related operations in a single place.
void print_app_start();
void print_app_usage(std::string command_name);
void print_app_arguments(Arguments args);
void print_download_error(const std::string& url, long http_code);
void print_download_success(
    const std::string& url,
    const std::filesystem::path& output_file_path,
    std::chrono::milliseconds elapsed
);
void print_download_start(const std::string& url);
void print_url_skipped(const std::string& url);
void print_libcurl_crash();
void print_callback_exception(std::exception e);
void print_app_finish();

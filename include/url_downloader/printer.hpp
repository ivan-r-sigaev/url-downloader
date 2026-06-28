#pragma once
#include "arguments.hpp"
#include <ostream>
#include <chrono>
#include <filesystem>

// Keeps all printing-related operations in a single place.
void print_app_start();
void print_app_usage(const std::string& command_name);
void print_unable_to_create_output(const std::filesystem::path& output_directory);
void print_app_arguments(const Arguments& args);
void print_download_error(const std::string& url, long http_code);
void print_download_success(
    const std::string& url,
    const std::filesystem::path& output_file_path,
    const std::chrono::milliseconds& elapsed
);
void print_download_start(const std::string& url);
void print_url_skipped(const std::string& url);
void print_callback_exception(const std::exception& e);
void print_app_finish();

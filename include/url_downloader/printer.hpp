#pragma once
#include "arguments.hpp"
#include <ostream>
#include <chrono>
#include <filesystem>

// Keeps all formatting-related operation in a single place.
class Printer {
public:
    // Default constructor. 
    // `Printer::out()` is `std::cout` and `Printer::err()` is `std::cerr`.
    Printer();

    Printer(const Printer&) = delete;
    Printer& operator=(const Printer&) = delete;
    Printer(Printer&&) = delete;
    Printer& operator=(Printer&&) = delete;
    
    // Returns printer's normal output stream.
    static std::ostream& out();
    // Returns printer's error output stream.
    static std::ostream& err();

    static void print_app_start();
    static void print_app_usage(std::string command_name);
    static void print_app_arguments(Arguments args);
    static void print_download_error(const std::string& url, long http_code);
    static void print_download_success(
        const std::string& url,
        const std::filesystem::path& output_file_path,
        std::chrono::milliseconds elapsed
    );
    static void print_download_start(const std::string& url);
    static void print_url_skipped(const std::string& url);
    static void print_libcurl_crash();
    static void print_app_finish();
private:
    std::ostream& _out;
    std::ostream& _err;
    // Returns printer instance.
    static Printer& get();
};

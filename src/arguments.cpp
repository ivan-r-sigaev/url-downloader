#include "url_downloader/arguments.hpp"
#include "url_downloader/printer.hpp"
#include <string>

Arguments::Arguments(
    std::filesystem::path _urls_file,
    std::filesystem::path _output_directory,
    long _max_parallel_downloads
) : 
    urls_file(std::move(_urls_file)),
    output_directory(std::move(_output_directory)),
    max_parallel_downloads(std::move(_max_parallel_downloads))
{}

Arguments Arguments::parse(int argc, char* argv[]) {
    std::string command_name = argc >= 1 ? std::string(argv[0]) : "url_downloader";

    if (argc < 4) {
        Printer::print_app_usage(command_name);
        std::exit(EXIT_FAILURE);
    }

    auto urls_file = std::filesystem::path(argv[1]);
    auto output_directory = std::filesystem::path(argv[2]);
    auto max_parallel_downloads = std::stol(std::string(argv[3]));

    return Arguments(urls_file, output_directory, max_parallel_downloads);
}


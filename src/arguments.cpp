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

std::optional<Arguments> Arguments::parse(int argc, char* argv[]) {
    if (argc < 4) {
        return std::nullopt;
    }
    
    std::filesystem::path urls_file;
    std::filesystem::path output_directory;
    long max_parallel_downloads;

    try {
        urls_file = std::filesystem::path(argv[1]);
        output_directory = std::filesystem::path(argv[2]);
        max_parallel_downloads = std::stol(std::string(argv[3]));
    } catch (std::exception e) {
        return std::nullopt;
    }

    return Arguments(urls_file, output_directory, max_parallel_downloads);
}


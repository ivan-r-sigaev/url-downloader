#pragma once
#include <filesystem>
#include <optional>

// Command line arguments for the program.
class Arguments {
public:
    // Path to the file that contains the urls to download.
    std::filesystem::path urls_file;
    // Path to the output directory to place download results into.
    std::filesystem::path output_directory;
    // Maximal allowed amount of concurrent download connections.
    long max_parallel_downloads;
public:
    // Explicit constructor.
    explicit Arguments(
        std::filesystem::path _urls_file,
        std::filesystem::path _output_directory,
        long _max_parallel_downloads
    );
    // Parses the arguments from the command line.
    static std::optional<Arguments> parse(int argc, char* argv[]);
};

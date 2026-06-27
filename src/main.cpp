#include "url_downloader/arguments.hpp"
#include "url_downloader/printer.hpp"
#include "url_downloader/parallel_download.hpp"

static std::vector<std::string> read_urls(const std::filesystem::path& urls_path);

int main(int argc, char* argv[]) {
    print_app_start();

    auto opt = Arguments::parse(argc, argv);
    if (!opt.has_value()) {
        auto command_name = argc >= 1 ? argv[0] : "url_downloader";
        print_app_usage(command_name);
        std::exit(EXIT_FAILURE);
    }

    auto args = opt.value();
    print_app_arguments(args);

    if (!std::filesystem::exists(args.output_directory)) {
        if (!std::filesystem::create_directories(args.output_directory)) {
            print_unable_to_create_output(args.output_directory);
            std::exit(EXIT_FAILURE);
        }
    }

    auto parallel_download = ParallelDownload();
    for (const auto& url : read_urls(args.urls_file)) {
        parallel_download.add(url, args.output_directory);
    }
    parallel_download.perform(args.max_parallel_downloads);

    print_app_finish();
    return 0;
}

/// Parses input file into url strings line by line. Empty lines are ignored.
static std::vector<std::string> read_urls(const std::filesystem::path& urls_path) {
    std::ifstream fs(urls_path.c_str(), std::fstream::in | std::fstream::binary);
    std::vector<std::string> out = {};
    for (std::string line; std::getline(fs, line);) {
        // Skip empty lines.
        auto is_whitespace = [](char ch) {
            return static_cast<bool>(std::isspace(static_cast<unsigned char>(ch)));
        };
        if (std::all_of(line.begin(), line.end(), is_whitespace)) {
            continue;
        }
        // Skip non-HTTP/HTTPS protocols.
        if (line.rfind("https://", 0) != 0 && line.rfind("http://", 0) != 0) {
            print_url_skipped(line);
            continue;
        }
        // Trim trailing whitespace
        const auto whitespace = " \t\n\r";
        auto end = line.find_last_not_of(whitespace);
        line = line.substr(0, end + 1);
        out.push_back(line);
    }
    return out;
}

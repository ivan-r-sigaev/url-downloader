#include <iostream>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <fstream>
#include <vector>
#include <algorithm>
#include <cctype>

/// Parses input file into url strings line by line. Empty lines are ignored.
std::vector<std::string> read_urls(std::filesystem::path urls_path) {
    std::ifstream fs(urls_path.c_str(), std::fstream::in | std::fstream::binary);
    std::vector<std::string> out = {};
    for (std::string line; std::getline(fs, line);) {
        // Skip whitespace.
        if (std::all_of(line.begin(), line.end(), isspace)) {
            continue;
        }
        // Skip non-HTTP/HTTPS protocols.
        if (line.rfind("https://", 0) != 0 || line.rfind("http://", 0) != 0) {
            std::cerr 
                << "Warning: URL skipped; Reason: unknown protocol, must be HTTP/HTTPS; URL: "
                << line
                << '\n';
            continue;
        }
        out.push_back(line);
    }
    return out;
}

int main(int argc, char* argv[]) {
    if (argc < 4) {
        std::string command_name = "url_downloader";
        if (argc >= 1) {
            command_name = std::string(argv[0]);
        }
        std::cerr << "Usage: " << command_name << " <urls_file> <out_dir> <parallel_download_count>\n";

        exit(EXIT_FAILURE);
    }
    auto urls_path = std::filesystem::path(argv[1]);
    auto out_dir_path = std::filesystem::path(argv[2]);
    auto parallel_download_count = std::stoi(std::string(argv[3]));

    auto urls = read_urls(urls_path);


    // TODO: download files over http...

    return 0;
}

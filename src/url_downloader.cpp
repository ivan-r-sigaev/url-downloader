#include <iostream>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <fstream>
#include <vector>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <sstream>
#include <ctime>
#include <curl_easy.h>
#include <curl_multi.h>
#include <iomanip>
#include <optional>
#include <utility>

class Arguments;

static std::string current_time_to_string();
static std::vector<std::string> read_urls(const std::filesystem::path& urls_path);
static void decode_url(std::string& text);
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
static void print_libcurl_crash();
static void print_app_finish();

// Command line arguments for the program.
class Arguments {
public:
    std::filesystem::path urls_file;
    std::filesystem::path output_directory;
    long max_parallel_downloads;
public:
    explicit Arguments(
        std::filesystem::path _urls_file,
        std::filesystem::path _output_directory,
        long _max_parallel_downloads
    ) : 
        urls_file(std::move(_urls_file)),
        output_directory(std::move(_output_directory)),
        max_parallel_downloads(std::move(_max_parallel_downloads))
    {}
    // Parses the arguments from the command line.
    static Arguments parse(int argc, char* argv[]);
};

// Internal structure used by ParallelDownload to manage a single download connection.
class DownloadInstance {
public:
    curl::curl_easy easy_handle;
    std::string url;
    std::filesystem::path output_file_path;
    std::ofstream output_file;
    std::optional<std::chrono::steady_clock::time_point> start_time;
public:
    explicit DownloadInstance(
        curl::curl_easy _easy_handle,
        std::string _url,
        std::filesystem::path _output_file_path,
        std::ofstream _output_file,
        std::optional<std::chrono::steady_clock::time_point> _start_time
    ):
        easy_handle(std::move(_easy_handle)),
        url(std::move(_url)),
        output_file_path(std::move(_output_file_path)),
        output_file(std::move(_output_file)),
        start_time(std::move(_start_time))
    {}
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
    // Enqueue a new url to be downloaded. Does not start downloading until `.perform()` is called.
    void add(const std::string& url, const std::filesystem::path& output_directory);
    // Begins downloading all of the enqueued urls.
    void perform(long max_parallel_downloads);
private:
    std::vector<std::optional<DownloadInstance>> downloads {};
private:
    static size_t header_callback(void *buffer, size_t size, size_t nitems, void *userdata);
    static size_t write_callback(void *ptr, size_t size, size_t nmemb, void *userdata);
    static std::string filename_from_url(const std::string& url);
};

int main(int argc, char* argv[]) {
    print_app_start();

    auto args = Arguments::parse(argc, argv);
    print_app_arguments(args);

    auto parallel_download = ParallelDownload();
    for (const auto& url : read_urls(args.urls_file)) {
        parallel_download.add(url, args.output_directory);
    }
    parallel_download.perform(args.max_parallel_downloads);

    print_app_finish();
    return 0;
}

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
            std::cerr
                << "Warning: URL skipped; Reason: unknown protocol, must be HTTP/HTTPS; URL: "
                << line
                << '\n';
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

static void sanitize_filename(std::string& filename) {
    std::replace_if(
        filename.begin(), 
        filename.end(),
        [](char c) {
            const auto forbidden = std::string("\\/:*?\"<>|\t\r\n");
            return c == '\0'
                || static_cast<unsigned char>(c) < 32
                || forbidden.find(c) != std::string::npos;
        },
        '_'
    );
}

static void create_file(std::ofstream& fs, std::filesystem::path path) {
    int postfix = 1;
    std::string name = path.filename().string();
    size_t delim = name.rfind('.');
    if (delim == std::string::npos) {
        delim = name.size();
    }
    std::string extension = name.substr(delim);
    std::string filename = name.substr(0, delim);
    while (std::filesystem::exists(path)) {
        path.replace_filename(filename + "(" + std::to_string(postfix) + ")" + extension);
        postfix++;
    }

    fs.open(path, std::fstream::out | std::fstream::binary);
}

Arguments Arguments::parse(int argc, char* argv[]) {
    std::string command_name = argc >= 1 ? std::string(argv[0]) : "url_downloader";

    if (argc < 4) {
        print_app_usage(command_name);
        std::exit(EXIT_FAILURE);
    }

    auto urls_file = std::filesystem::path(argv[1]);
    auto output_directory = std::filesystem::path(argv[2]);
    auto max_parallel_downloads = std::stol(std::string(argv[3]));

    return Arguments(urls_file, output_directory, max_parallel_downloads);
}

void ParallelDownload::add(const std::string& url, const std::filesystem::path& output_directory) {
    auto output_file_path = output_directory;
    output_file_path /= filename_from_url(url);

    auto& download = this->downloads.emplace_back(
        std::in_place,
        curl::curl_easy(),
        url,
        output_file_path,
        std::ofstream(),
        std::nullopt
    ).value();

    // Set output_file to throw on any errors to simplify error handling.
    download.output_file.exceptions(std::ios::failbit | std::ios::badbit);

    download.easy_handle.add<CURLOPT_URL>(url.c_str());
    download.easy_handle.add<CURLOPT_WRITEFUNCTION>(ParallelDownload::write_callback);
    download.easy_handle.add<CURLOPT_HEADERFUNCTION>(ParallelDownload::header_callback);
}

void ParallelDownload::perform(long max_parallel_downloads) {
    curl::curl_multi multi_handle {};
    multi_handle.add<CURLMOPT_MAX_TOTAL_CONNECTIONS>(max_parallel_downloads);

    for (auto& opt : this->downloads) {
        auto& download = opt.value();
        auto userdata = static_cast<void*>(&opt);
        download.easy_handle.add<CURLOPT_WRITEDATA>(userdata);
        download.easy_handle.add<CURLOPT_HEADERDATA>(userdata);
        download.easy_handle.add<CURLOPT_PRIVATE>(userdata);
        multi_handle.add(download.easy_handle);
    }

    do {
        multi_handle.perform();
        multi_handle.wait(nullptr, 0, 1000, nullptr);

        while (true) {
            auto result = multi_handle.get_next_finished();
            if (result == nullptr) {
                break;
            }
            auto msg = result.get();
            if (msg->get_message() == CURLMSG_DONE) {
                auto now = std::chrono::steady_clock::now();

                if (msg->get_code() != CURLE_OK) {
                    print_libcurl_crash();
                    std::exit(EXIT_FAILURE);
                }
                
                auto userdata = msg->get_handler()->get_info<CURLINFO_PRIVATE>().get();
                auto& opt = *static_cast<std::optional<DownloadInstance>*>(userdata);
                auto& download = opt.value();

                auto http_code = download.easy_handle.get_info<CURLINFO_RESPONSE_CODE>().get();
                if (http_code != 200) {
                    print_download_error(download.url, http_code);
                } else {
                    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - download.start_time.value());
                    print_download_success(download.url, download.output_file_path, elapsed);
                }

                multi_handle.remove(download.easy_handle);
                opt = std::nullopt;
            }
        }
    } while (multi_handle.get_active_transfers() > 0);

    this->downloads.clear();
    this->downloads.shrink_to_fit();
}

static std::optional<std::string> parse_content_disposition(const std::string& value) {
    auto sstream = std::stringstream(value);
    auto token = std::string();
    std::optional<std::string> filename = std::nullopt;

    while (std::getline(sstream, token, ';')) {
        const auto whitespace = " \t";
        token = token.substr(token.find_first_not_of(whitespace));
        const auto filename_field = std::string("filename=");
        const auto utf8_filename_field = std::string("filename*=");
        if (token.rfind(utf8_filename_field, 0) != std::string::npos) {
            auto encoded = token.substr(utf8_filename_field.size());
            // Encoding format: charset'language'encoded-value
            auto first_quote = encoded.find('\'');
            if (first_quote != std::string::npos) {
                auto second_quote = encoded.find('\'', first_quote + 1);
                if (second_quote != std::string::npos) {
                    auto encoded_value = encoded.substr(second_quote + 1);
                    // Unquote if needed.
                    if (!encoded_value.empty() && encoded_value[0] == '"' && encoded_value.back() == '"') {
                        encoded_value = encoded_value.substr(1, encoded_value.length() - 2);
                    }
                    decode_url(encoded_value);
                    sanitize_filename(encoded_value);
                    if (!encoded_value.empty()) {
                        filename = encoded_value;
                        break;
                    }
                }
            }
        } else if (token.rfind(filename_field, 0) != std::string::npos) {
            token = token.substr(filename_field.size());
            if (!token.empty() && token[0] == '"' && token.back() == '"') {
                token = token.substr(1, token.length() - 2);
            }
            sanitize_filename(token);
            if (!token.empty()) {
                filename = token;
            }
        }
    }

    return filename;
}

size_t ParallelDownload::header_callback(void *buffer, size_t size, size_t nitems, void *userdata) {
    // Throwing outside from a C++ callback that is called by C code (libcurl) is UNDEFINED BEHAVIOUR.
    try {
        auto& opt = *static_cast<std::optional<DownloadInstance>*>(userdata);
        auto& download = opt.value();
    
        if (!download.start_time.has_value()) {
            download.start_time = std::optional{std::chrono::steady_clock::now()};
            print_download_start(download.url);
        }
    
        std::string header(static_cast<char*>(buffer), size * nitems);
        auto delim = header.find(':');
        if (delim != std::string::npos) {
            auto key = header.substr(0, delim);
            auto value = header.substr(delim + 1);
            std::transform(
                key.begin(),
                key.end(),
                key.begin(),
                [] (unsigned char c) {
                    return std::tolower(c);
                }
            );
            if (key == "content-disposition") {
                auto filename = parse_content_disposition(value);
                if (filename.has_value()) {
                    download.output_file_path.replace_filename(filename.value());
                }
            }
        }
        return size * nitems;
    } catch (std::exception e) {
        std::cerr << e.what() << '\n';
        std::exit(EXIT_FAILURE);
    } catch (...) {
        std::exit(EXIT_FAILURE);
    }
}

size_t ParallelDownload::write_callback(void *ptr, size_t size, size_t nmemb, void *userdata) {
    // Throwing outside from a C++ callback that is called by C code (libcurl) is UNDEFINED BEHAVIOUR.
    try {
        auto& opt = *static_cast<std::optional<DownloadInstance>*>(userdata);
        auto& download = opt.value();

        if (!download.start_time.has_value()) {
            download.start_time = std::chrono::steady_clock::now();
            print_download_start(download.url);
        }

        if (!download.output_file.is_open()) {
            create_file(download.output_file, download.output_file_path);
        }

        if (ptr != nullptr && nmemb * size != 0) {
            download.output_file.write(static_cast<char*>(ptr), nmemb * size);
        }

        return nmemb * size;
    } catch (std::exception e) {
        std::cerr << e.what() << '\n';
        std::exit(EXIT_FAILURE);
    } catch (...) {
        std::exit(EXIT_FAILURE);
    }
}

static void decode_url(std::string& text) {
    int out_size = 0;
    auto cstr = curl_easy_unescape(nullptr, text.c_str(), 0, &out_size);
    if (cstr) {
        text = std::string(cstr, out_size);
        curl_free(cstr);
    }
}

std::string ParallelDownload::filename_from_url(const std::string& url) {
    auto out = url.substr(0, url.find('#'));
    out = out.substr(0, out.find('?'));
    out = out.substr((std::min)(out.rfind('/') + 1, out.length()));
    if (out.empty()) {
        out = "placeholder.txt";
    } else {
        decode_url(out);
        sanitize_filename(out);
    }
    return out;
}

static void print_app_start() {
    std::cout << current_time_to_string() << " Url Downloader Started" << std::endl;
}

static void print_app_arguments(Arguments args) {
    std::cout
        << current_time_to_string()
        << " urls-path: " << args.urls_file
        << "; out-dir: " << args.output_directory
        << "; parallel-download-count: " << args.max_parallel_downloads
        << std::endl;
}

static void print_app_usage(std::string command_name) {
    std::cerr << "Usage: " << command_name << " <urls_file> <out_dir> <parallel_download_count>\n";
}

static void print_download_error(const std::string& url, long http_code) {
    std::cerr 
        << "Error: Failed to obtain URL contents; Code: "
        << http_code 
        << "; URL: "
        << url 
        << '\n';
}

static void print_download_success(
    const std::string& url,
    const std::filesystem::path& output_file_path,
    std::chrono::milliseconds elapsed
) {
    std::cout
        << current_time_to_string()
        << " " << output_file_path
        << " - finished downloading in " << elapsed.count() << " ms"
        << std::endl;
}

static void print_download_start(const std::string& url) {
    std::cout
        << current_time_to_string()
        << " " << url
        << " - started downloading"
        << std::endl;
}

static void print_libcurl_crash() {
    std::cerr 
        << current_time_to_string()
        << " internal libcurl error, crashing."
        << '\n';
}

static void print_app_finish() {
    std::cout << current_time_to_string() << " Url Downloader Finished" << std::endl;
}

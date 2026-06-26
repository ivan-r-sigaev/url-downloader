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
#include <cstdio>
#include <ctime>
#include <curl_easy.h>
#include <curl_multi.h>

/// Handle to manage an instance of downloading a file from URL.
class DownloadHandle {
public:
    curl::curl_easy easy_handle;
    std::string url;
    std::filesystem::path out_path{};
    std::ofstream out_file{};
    bool has_started = false;
    std::chrono::steady_clock::time_point start_time{};

    explicit DownloadHandle(
        std::string _url,
        std::filesystem::path _out_path
    ) : url(_url), out_path(_out_path) {}
};

static std::string current_time_to_string();
static std::vector<std::string> read_urls(const std::filesystem::path& urls_path);
static void sanitize_filename(std::string& filename);
static size_t my_header_callback(char *buffer, size_t size, size_t nitems, void *userdata);
static void create_file(std::ofstream& fs, std::filesystem::path path);
static size_t my_write_callback(char *ptr, size_t size, size_t nmemb, void *userdata);
static void decode_url(std::string& text);
static std::string filename_from_url(const std::string& url);
static void print_usage(std::string command_name);

typedef size_t (*curlopt_writefunction_type)(void *ptr, size_t size, size_t nmemb, void *userdata);
typedef size_t (*curlopt_headerfunction_type)(void *buffer, size_t size, size_t nitems, void *userdata);

int main(int argc, char* argv[]) {
    std::cout << current_time_to_string() << " Url Downloader Started" << std::endl;

    std::string command_name = argc >= 1 ? std::string(argv[0]) : "url_downloader";
    
    if (argc < 4) {
        print_usage(command_name);
        curl_global_cleanup();
        std::exit(EXIT_FAILURE);
    }

    auto urls_path = std::filesystem::path(argv[1]);
    auto out_dir_path = std::filesystem::path(argv[2]);
    auto parallel_download_count = std::stoi(std::string(argv[3]));
    std::cout
        << current_time_to_string()
        << " urls-path: " << urls_path
        << "; out-dir: " << out_dir_path
        << "; parallel-download-count: " << parallel_download_count
        << std::endl;

    auto multi_handle = curl::curl_multi();
    multi_handle.add<CURLMOPT_MAX_TOTAL_CONNECTIONS>((long)parallel_download_count);
    auto handles = std::vector<DownloadHandle>();
    {
        auto urls = read_urls(urls_path);
        handles.reserve(urls.size());  // `handles` must not be reallocated
        for (const auto& url : urls) {
            auto out_path = out_dir_path;
            out_path /= filename_from_url(url);

            auto handle = &handles.emplace_back(url, out_path);
            auto usrptr = static_cast<void*>(handle);
            auto easy_handle = &handle->easy_handle;
            
            easy_handle->add<CURLOPT_URL>(url.c_str());
            easy_handle->add<CURLOPT_WRITEFUNCTION>((curlopt_writefunction_type)my_write_callback);
            easy_handle->add<CURLOPT_HEADERFUNCTION>((curlopt_headerfunction_type)my_header_callback);
            easy_handle->add<CURLOPT_WRITEDATA>(usrptr);
            easy_handle->add<CURLOPT_HEADERDATA>(usrptr);
            easy_handle->add<CURLOPT_PRIVATE>(usrptr);
            multi_handle.add(*easy_handle);
        }
    }
    
    int running_handles_count = 0;
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
                // TODO: check result
                // assert_easy_curl(msg->data.result);
                auto now = std::chrono::steady_clock::now();
                auto easy_handle = msg->get_handler();
                auto http_code = easy_handle->get_info<CURLINFO_RESPONSE_CODE>().get();
                auto usrptr = easy_handle->get_info<CURLINFO_PRIVATE>().get();
                DownloadHandle* handle = static_cast<DownloadHandle*>(usrptr);
                handle->out_file.close();
                if (http_code != 200) {
                    std::cerr 
                        << "Error: Failed to obtain URL contents; Code: "
                        << http_code 
                        << "; URL: "
                        << handle->out_path 
                        << '\n';
                } else {
                    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - handle->start_time).count();
                    std::cout
                        << current_time_to_string()
                        << " " << handle->out_path
                        << " - finished downloading in " << elapsed << " ms\n";
                }
            }
            std::cout << std::flush;
        }
    } while (multi_handle.get_active_transfers() > 0);

    std::cout << current_time_to_string() << " Url Downloader Finished" << std::endl;
    return 0;
}

static std::string current_time_to_string() {
    auto currentTime = std::chrono::system_clock::now();
    const auto size = 80;
    char buffer[size];
    
    auto transformed = currentTime.time_since_epoch().count() / 1000000;
    auto millis = transformed % 1000;
    
    std::time_t tt = std::chrono::system_clock::to_time_t(currentTime);
    auto timeinfo = std::localtime(&tt);
    std::strftime(buffer, size, "%F %H:%M:%S", timeinfo);
    std::sprintf(buffer, "%s:%03d", buffer, (int)millis);
    
    return std::string(buffer);
}

/// Parses input file into url strings line by line. Empty lines are ignored.
static std::vector<std::string> read_urls(const std::filesystem::path& urls_path) {
    std::ifstream fs(urls_path.c_str(), std::fstream::in | std::fstream::binary);
    std::vector<std::string> out = {};
    for (std::string line; std::getline(fs, line);) {
        // Skip empty lines.
        if (std::all_of(line.begin(), line.end(), isspace)) {
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

static size_t my_header_callback(char *buffer, size_t size, size_t nitems, void *userdata) {
    auto handle = static_cast<DownloadHandle*>(userdata);

    if (!handle->has_started) {
        handle->start_time = std::chrono::steady_clock::now();
        handle->has_started = true;
        std::cout
            << current_time_to_string()
            << " " << handle->url
            << " - started downloading"
            << std::endl;
    }

    std::string header(buffer, size * nitems);
    auto delim = header.find(':');
    if (delim != std::string::npos) {
        auto key = header.substr(0, delim);
        auto value = header.substr(delim + 1);
        std::transform(
            key.begin(), key.end(), key.begin(), 
            [](unsigned char c){ return std::tolower(c); }
        );
        if (key == "content-disposition") {
            auto sstream = std::stringstream(value);
            auto token = std::string();

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
                                handle->out_path.replace_filename(encoded_value);
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
                        handle->out_path.replace_filename(token);
                    }
                }
            }
        }
    }
    return size * nitems;
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

    // TODO: should I check success?
    fs.open(path, std::fstream::out | std::fstream::binary);
}

static size_t my_write_callback(char *ptr, size_t size, size_t nmemb, void *userdata) {
    auto handle = static_cast<DownloadHandle*>(userdata);

    if (!handle->out_file.is_open()) {
        create_file(handle->out_file, handle->out_path);
    }

    if (ptr != nullptr && nmemb != 0) {
        // TODO: Should I check success?
        handle->out_file.write(ptr, nmemb);
    }

    return nmemb;
}

static void decode_url(std::string& text) {
    int out_size = 0;
    auto cstr = curl_easy_unescape(nullptr, text.c_str(), 0, &out_size);
    if (cstr) {
        text = std::string(cstr, out_size);
        curl_free(cstr);
    }
}

static std::string filename_from_url(const std::string& url) {
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

static void print_usage(std::string command_name) {
    std::cerr << "Usage: " << command_name << " <urls_file> <out_dir> <parallel_download_count>\n";
}

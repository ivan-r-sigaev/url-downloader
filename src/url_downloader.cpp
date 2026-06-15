#include <iostream>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <fstream>
#include <vector>
#include <algorithm>
#include <cctype>
#include <curl/curl.h>
#include <chrono>
#include <sstream>
#include <cstdio>
#include <ctime>

/// Handle to manage an instance of downloading a file from URL.
class DownloadHandle {
public:
    std::string url;
    CURLU* url_handle;
    std::filesystem::path out_path{};
    std::ofstream out_file{};
    bool has_started = false;
    std::chrono::steady_clock::time_point start_time{};

    explicit DownloadHandle(
        std::string _url,
        CURLU* _url_handle,
        std::filesystem::path _out_path
    ) : url(_url), url_handle(_url_handle), out_path(_out_path) {}
};

static std::string current_time_to_string();
static std::vector<std::string> read_urls(const std::filesystem::path& urls_path);
static void sanitize_filename(std::string& filename);
static size_t header_callback(char *buffer, size_t size, size_t nitems, void *userdata);
static void create_file(std::ofstream& fs, std::filesystem::path path);
static size_t write_callback(char *ptr, size_t size, size_t nmemb, void *userdata);
static void decode_url(std::string& text);
static std::string filename_from_url(const std::string& url);
static void print_usage(std::string command_name);

#define assert_easy_curl(code) _assert_easy_curl(code, __LINE__)
void _assert_easy_curl(CURLcode code, int line) {
    if (code != CURLE_OK) {
        std::cerr 
            << current_time_to_string() 
            << " Error: libcurl easy error, aborting"
            << " Message: " << curl_easy_strerror(code)
            << " Line: " << line
            << '\n';
        std::abort();
    }
}

#define assert_multi_curl(code) _assert_multi_curl(code, __LINE__)
void _assert_multi_curl(CURLMcode code, int line) {
    if (code != CURLM_OK) {
        std::cerr
            << current_time_to_string() 
            << " Error: libcurl multi error, aborting"
            << " Message: " << curl_multi_strerror(code)
            << " Line: " << line
            << '\n';
        std::abort();
    }
}

#define assert_url_curl(code) _assert_url_curl(code, __LINE__)
void _assert_url_curl(CURLUcode code, int line) {
    if (code != CURLUE_OK) {
        std::cerr
            << current_time_to_string() 
            << " Error: libcurl url error, aborting"
            << " Message: " << curl_url_strerror(code)
            << " Line: " << line
            << '\n';
        std::abort();
    }
}

#define assert_nonnull_curl(ptr) _assert_nonnull_curl(ptr, __LINE__)
void _assert_nonnull_curl(void* ptr, int line) {
    if (ptr == nullptr) {
        std::cerr
            << current_time_to_string()
            << " Error: internal libcurl error, aborting"
            << " Line: " << line
            << '\n';
        std::abort();
    }
}

int main(int argc, char* argv[]) {
    std::cout << current_time_to_string() << " Url Downloader Started" << std::endl;

    assert_easy_curl(curl_global_init(CURL_GLOBAL_ALL));
    

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

    CURLM* multi_handle = curl_multi_init();
    assert_nonnull_curl(multi_handle);
    assert_multi_curl(curl_multi_setopt(multi_handle, CURLMOPT_MAX_TOTAL_CONNECTIONS, (long)parallel_download_count));
    auto handles = std::vector<DownloadHandle>();
    {
        auto urls = read_urls(urls_path);
        handles.reserve(urls.size());  // `handles` must not be reallocated
        for (const auto& url : urls) {
            auto out_path = out_dir_path;
            out_path /= filename_from_url(url);
            
            auto url_handle = curl_url();
            assert_nonnull_curl(url_handle);
            assert_url_curl(curl_url_set(url_handle, CURLUPART_URL, url.c_str(), 0));
    
            auto handle = static_cast<void*>(&handles.emplace_back(url, url_handle, out_path));
    
            auto easy_handle = curl_easy_init();
            assert_nonnull_curl(easy_handle);
            assert_easy_curl(curl_easy_setopt(easy_handle, CURLOPT_CURLU, url_handle));
            assert_easy_curl(curl_easy_setopt(easy_handle, CURLOPT_WRITEFUNCTION, &write_callback));
            assert_easy_curl(curl_easy_setopt(easy_handle, CURLOPT_HEADERFUNCTION, &header_callback));
            assert_easy_curl(curl_easy_setopt(easy_handle, CURLOPT_WRITEDATA, handle));
            assert_easy_curl(curl_easy_setopt(easy_handle, CURLOPT_HEADERDATA, handle));
            assert_easy_curl(curl_easy_setopt(easy_handle, CURLOPT_PRIVATE, handle));
            assert_multi_curl(curl_multi_add_handle(multi_handle, easy_handle));
        }
    }
    
    int running_handles_count = 0;
    do {
        assert_multi_curl(curl_multi_perform(multi_handle, &running_handles_count));
        assert_multi_curl(curl_multi_wait(multi_handle, nullptr, 0, 1000, nullptr));

        while (true) {
            int msgs_left = 0;
            CURLMsg* msg = curl_multi_info_read(multi_handle, &msgs_left);
            if (msg == nullptr) {
                break;
            }
            if (msg->msg == CURLMSG_DONE) {
                assert_easy_curl(msg->data.result);
                auto now = std::chrono::steady_clock::now();
                auto easy_handle = msg->easy_handle;
                long http_code = 0;
                DownloadHandle* handle = nullptr;
                assert_easy_curl(curl_easy_getinfo(easy_handle, CURLINFO_RESPONSE_CODE, &http_code));
                assert_easy_curl(curl_easy_getinfo(easy_handle, CURLINFO_PRIVATE, &handle));
                handle->out_file.close();
                assert_multi_curl(curl_multi_remove_handle(multi_handle, easy_handle));
                curl_url_cleanup(handle->url_handle);
                curl_easy_cleanup(easy_handle);
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
    } while (running_handles_count > 0);

    assert_multi_curl(curl_multi_cleanup(multi_handle));
    curl_global_cleanup();

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
        // Trim trailing newline
        line.pop_back();
        out.push_back(line);
    }
    return out;
}

static void sanitize_filename(std::string& filename) {
    // May want to sanitize filename here...
}

static size_t header_callback(char *buffer, size_t size, size_t nitems, void *userdata) {
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
        auto filename = std::string();
        std::transform(
            value.begin(), value.end(), value.begin(), 
            [](unsigned char c){ return std::tolower(c); }
        );
        if (key == "content-disposition") {
            auto sstream = std::stringstream(value);
            auto token = std::string();

            while (std::getline(sstream, token, ';')) {
                const auto whitespace = " \t";
                token = token.substr(token.find_first_not_of(whitespace));
                const auto filename_field = std::string("filename=");
                if (token.rfind(filename_field) != std::string::npos) {
                    token = token.substr(filename_field.size());
                    if (!token.empty() && token[0] == '"' && token.back() == '"') {
                        token = token.substr(1, token.length() - 2);
                    }
                    sanitize_filename(token);
                    filename = token;
                }
                const auto utf8_filename_field = std::string("filename*=");
                if (token.rfind(utf8_filename_field) != std::string::npos) {
                    // TODO: parse utf-8 filename...
                }
            }
        }
    }
    return size * nitems;
}

static void create_file(std::ofstream& fs, std::filesystem::path path) {
    fs.open(path, std::fstream::out | std::fstream::binary);
    if (!fs.good()) {
        return;
    }

    int postfix = 1;
    std::string filename = path.filename().string();
    size_t delim = filename.rfind('.');
    std::string extension = filename.substr(delim);
    filename = filename.substr(0, delim);
    do {
        path.replace_filename(filename + "(" + std::to_string(postfix) + ")" + extension);
        fs.open(path, std::fstream::out | std::fstream::binary);
        postfix++;
    } while (!fs.good());
}

static size_t write_callback(char *ptr, size_t size, size_t nmemb, void *userdata) {
    auto handle = static_cast<DownloadHandle*>(userdata);

    if (!handle->out_file.is_open()) {
        create_file(handle->out_file, handle->out_path);
    }

    if (ptr != nullptr && nmemb != 0) {
        // Should I check success?
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

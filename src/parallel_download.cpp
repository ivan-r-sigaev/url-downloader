#include "url_downloader/parallel_download.hpp"
#include "url_downloader/printer.hpp"
#include <curl_multi.h>

// Replaces the disallowed characters in the filename with '_'.
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

// Creates a opens the ofstream at given path.
// If path already exists adds "(1)", "(2)" and so on to the filename.
static void create_file(std::ofstream& fs, std::filesystem::path& path) {
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

// Decodes the url-encoded string.
static void decode_url(std::string& text) {
    int out_size = 0;
    auto cstr = curl_easy_unescape(nullptr, text.c_str(), 0, &out_size);
    if (cstr) {
        text = std::string(cstr, out_size);
        curl_free(cstr);
    }
}

// Recovers some basic filename from the url.
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

// Removes outer quotes from text if present.
static void simple_unquote(std::string& text) {
    if (!text.size() >= 2 && text[0] == '"' && text.back() == '"') {
        text = text.substr(1, text.length() - 2);
    }
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
            // Encoding format: charset'language'encoded-name
            auto x1 = encoded.find('\'');
            if (x1 == std::string::npos) {
                continue;
            }
            auto x2 = encoded.find('\'', x1 + 1);
            if (x2 == std::string::npos) {
                continue;
            }
            auto value = encoded.substr(x2 + 1);
            simple_unquote(value);
            decode_url(value);
            sanitize_filename(value);
            if (!value.empty()) {
                filename = value;
                break;
            }
        } else if (token.rfind(filename_field, 0) != std::string::npos) {
            auto value = token.substr(filename_field.size());
            simple_unquote(value);
            sanitize_filename(value);
            if (!value.empty()) {
                filename = value;
            }
        }
    }

    return filename;
}

DownloadInstance::DownloadInstance(
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
                    Printer::print_libcurl_crash();
                    std::exit(EXIT_FAILURE);
                }
                
                auto userdata = msg->get_handler()->get_info<CURLINFO_PRIVATE>().get();
                auto& opt = *static_cast<std::optional<DownloadInstance>*>(userdata);
                auto& download = opt.value();

                auto http_code = download.easy_handle.get_info<CURLINFO_RESPONSE_CODE>().get();
                if (http_code != 200) {
                    Printer::print_download_error(download.url, http_code);
                } else {
                    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - download.start_time.value());
                    Printer::print_download_success(download.url, download.output_file_path, elapsed);
                }

                multi_handle.remove(download.easy_handle);
                opt = std::nullopt;
            }
        }
    } while (multi_handle.get_active_transfers() > 0);

    this->downloads.clear();
    this->downloads.shrink_to_fit();
}

size_t ParallelDownload::header_callback(void *buffer, size_t size, size_t nitems, void *userdata) {
    // Throwing outside from a C++ callback that is called by C code (libcurl) is UNDEFINED BEHAVIOUR.
    try {
        auto& opt = *static_cast<std::optional<DownloadInstance>*>(userdata);
        auto& download = opt.value();
    
        if (!download.start_time.has_value()) {
            download.start_time = std::optional{std::chrono::steady_clock::now()};
            Printer::print_download_start(download.url);
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
            Printer::print_download_start(download.url);
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

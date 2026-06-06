add_executable(url_downloader)

set_property(TARGET url_downloader PROPERTY CXX_STANDARD 17)
target_sources(url_downloader PRIVATE "url_downloader.cpp")

FetchContent_MakeAvailable(curl)
target_link_libraries(url_downloader PRIVATE libcurl)

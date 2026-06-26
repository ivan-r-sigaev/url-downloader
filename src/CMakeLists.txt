add_executable(url_downloader)

set_property(TARGET url_downloader PROPERTY CXX_STANDARD 17)
target_sources(url_downloader
    PRIVATE
        main.cpp
        arguments.cpp
        printer.cpp
        parallel_download.cpp
)
target_include_directories(url_downloader
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)
# Include curlcpp's headers
target_include_directories(url_downloader PRIVATE ${curlcpp_SOURCE_DIR}/include)
target_link_libraries(url_downloader PRIVATE libcurl curlcpp)

cmake_minimum_required(VERSION 3.16)
include(FetchContent)

project(url_downloader LANGUAGES CXX)

FetchContent_Declare(
    curl
    GIT_REPOSITORY https://github.com/curl/curl.git
    GIT_TAG        curl-8_8_0
    GIT_SHALLOW    TRUE
)

# https://github.com/curl/curl/blob/master/docs/INSTALL-CMAKE.md#cmake-build-options
set(BUILD_CURL_EXE     OFF CACHE BOOL "" FORCE)  # No curl executable needed.
set(BUILD_SHARED_LIBS  OFF CACHE BOOL "" FORCE)  # No dynamic linking.
if (WIN32)  # HTTPS needed.
    set(CURL_USE_OPENSSL  OFF CACHE BOOL "" FORCE)
    set(CURL_USE_SCHANNEL ON  CACHE BOOL "" FORCE)
else()
    set(CURL_USE_OPENSSL  ON  CACHE BOOL "" FORCE)
    set(CURL_USE_SCHANNEL OFF CACHE BOOL "" FORCE)
endif()
set(HTTP_ONLY          ON  CACHE BOOL "" FORCE)  # Only HTTP/HTTPS needed.

add_subdirectory(src)

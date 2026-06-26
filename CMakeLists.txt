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
FetchContent_MakeAvailable(curl)

# Need to provide values manually to curlcpp.
set(CURL_INCLUDE_DIR    "${curl_SOURCE_DIR}/include" CACHE PATH "" FORCE)
set(CURL_LIBRARY        libcurl_static               CACHE STRING "" FORCE)
set(CURL_VERSION_STRING "8.8.0"                      CACHE STRING "" FORCE)

FetchContent_Declare(
    curlcpp
    GIT_REPOSITORY https://github.com/ivan-r-sigaev/curlcpp.git
    GIT_TAG        fix-curlinfo-private-ub
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(curlcpp)

add_subdirectory(src)

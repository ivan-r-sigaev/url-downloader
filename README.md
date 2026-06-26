# url-downloader

[![Build Status](https://github.com/ivan-r-sigaev/url-downloader/actions/workflows/ci.yml/badge.svg)](https://github.com/ivan-r-sigaev/url-downloader/actions)
[![License](https://img.shields.io/badge/License-MIT-yellow.svg)](./LICENSE)

A simple command line tool to download multiple files from a list of urls.

## Usage

Run program like so:

```
url-downloader <urls_file> <out_dir> <parallel_download_count>
```
where:
- `<urls_file>` - path to text file containing newline separated urls to download.
- `<out_dir>` - path to directory to download files into.
- `<parallel_download_count>` - maximum allowed number of concurrent downloads. 

## Dependencies

All dependencies are automatically built into the executable by CMake.

List of dependencies:

- libcurl 8.8.0: https://github.com/curl/curl/releases/tag/curl-8_8_0
- curlcpp 3.1: https://github.com/JosephP91/curlcpp/releases/tag/3.1

## Building from source

```
git clone https://github.com/ivan-r-sigaev/url-downloader.git
cd url-downloader

# Create build directory
mkdir build
cd build

# Configure CMake
cmake ..

# Build the executable
cmake --build . --config Release --parallel
```

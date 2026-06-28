# url-downloader

[![Build Status](https://github.com/ivan-r-sigaev/url-downloader/actions/workflows/ci.yml/badge.svg)](https://github.com/ivan-r-sigaev/url-downloader/actions)
[![License](https://img.shields.io/badge/License-MIT-yellow.svg)](./LICENSE)

A command-line tool that can download files from several urls at once. 

**This is a reference implementation that exists only for demonstrative purposes.**

## Usage

Run program like so:

```
url-downloader <urls-file-path> <ouput-direcotry-path> <max-parallel-downloads>
```
where:
- `<urls-file-path>` - path to the text file containing newline separated urls to be downloaded.
- `<ouput-direcotry-path>` - path to the directory to download files into (will be created if does not exist).
- `<max-parallel-downloads>` - maximum allowed number of concurrent downloads.

## Dependencies

All dependencies are automatically fetched and included into the project by CMake at build time.

List of dependencies:

- libcurl 8.8.0: https://github.com/curl/curl/releases/tag/curl-8_8_0
- curlcpp: https://github.com/JosephP91/curlcpp/tree/cea6852eb24a7a03d524d61ea6687c3f0e987bfd

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

## What I learned

The purpose of this tool was to implement downloading files over network using C++17. During development I:

- Learned how to use [libcurl](https://github.com/curl/curl) for basic download operations.
- Learned how to use libcurl C++ wrappers such as [curlcpp](https://github.com/JosephP91/curlcpp).
- Opened several bug issues in [curlcpp](https://github.com/JosephP91/curlcpp)'s repository to help develop the project (see [first issue](https://github.com/JosephP91/curlcpp/issues/165), [second issue](https://github.com/JosephP91/curlcpp/issues/167)).
- Opened a pull request ([see](https://github.com/JosephP91/curlcpp/pull/166)) to fix an issue and had it successfully merged.
- Create a [GitHub Actions CI script](./.github/workflows/ci.yml) to automatically verify that the project's code successfully compiles for both Windows and Linux.
- Used branch protection to ensure that the main branch only has vaild code.

cmake_minimum_required(VERSION 4.0)
project(rgTorrent C)

set(CMAKE_C_STANDARD 99)

find_package(OpenSSL REQUIRED)

add_executable(rgTorrent main.c bencoding/bencoder.c bencoding/bencode_parser.c helpers/helpers.c)

target_include_directories(rgTorrent PRIVATE helpers bencoding)
target_link_libraries(rgTorrent OpenSSL::SSL OpenSSL::Crypto)

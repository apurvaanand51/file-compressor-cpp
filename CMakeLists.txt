cmake_minimum_required(VERSION 3.16)
project(file_compressor VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(huffman src/huffman.cpp)
target_include_directories(huffman PUBLIC include)

if(MSVC)
    target_compile_options(huffman PRIVATE /W4 /WX)
else()
    target_compile_options(huffman PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

add_executable(file-compressor src/main.cpp)
target_link_libraries(file-compressor PRIVATE huffman)

include(CTest)
if(BUILD_TESTING)
    add_executable(huffman-tests tests/test_huffman.cpp)
    target_link_libraries(huffman-tests PRIVATE huffman)
    add_test(NAME huffman-round-trip COMMAND huffman-tests)
endif()

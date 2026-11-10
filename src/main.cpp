#include "huffman.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void print_usage() {
    std::cout << "File Compressor - Huffman coding\n\n"
              << "Usage:\n"
              << "  file-compressor compress <input-file> [output-file]\n"
              << "  file-compressor decompress <input-file> [output-file]\n\n"
              << "If omitted, the output is <input-file>.huf when compressing, or\n"
              << "<input-file>.restored when decompressing.\n";
}

std::filesystem::path default_output(const std::string& command,
                                     const std::filesystem::path& input) {
    if (command == "compress") {
        return std::filesystem::path(input.string() + ".huf");
    }
    return std::filesystem::path(input.string() + ".restored");
}

void ensure_distinct_files(const std::filesystem::path& input,
                           const std::filesystem::path& output) {
    const auto absolute_input = std::filesystem::absolute(input).lexically_normal();
    const auto absolute_output = std::filesystem::absolute(output).lexically_normal();
    if (absolute_input == absolute_output ||
        (std::filesystem::exists(output) && std::filesystem::equivalent(input, output))) {
        throw std::runtime_error("Input and output must be different files.");
    }
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc == 2 && std::string(argv[1]) == "--help") {
        print_usage();
        return 0;
    }
    if (argc < 3 || argc > 4) {
        print_usage();
        return 2;
    }

    const std::string command = argv[1];
    if (command != "compress" && command != "decompress") {
        print_usage();
        return 2;
    }

    try {
        const std::filesystem::path input_path = argv[2];
        const std::filesystem::path output_path =
            argc == 4 ? std::filesystem::path(argv[3]) : default_output(command, input_path);
        ensure_distinct_files(input_path, output_path);

        std::ifstream input(input_path, std::ios::binary);
        if (!input) {
            throw std::runtime_error("Could not open input file: " + input_path.string());
        }
        std::ofstream output(output_path, std::ios::binary | std::ios::trunc);
        if (!output) {
            throw std::runtime_error("Could not create output file: " + output_path.string());
        }

        if (command == "compress") {
            huffman::compress(input, output);
        } else {
            huffman::decompress(input, output);
        }
        output.close();
        if (!output) {
            throw std::runtime_error("Failed to finish writing output file.");
        }
        std::cout << (command == "compress" ? "Compressed" : "Decompressed")
                  << " successfully: " << output_path.string() << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }
}

#include "huffman.hpp"

#include <cstdint>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {

std::string compress_bytes(const std::string& original) {
    std::istringstream input(original, std::ios::binary);
    std::ostringstream compressed(std::ios::binary);
    huffman::compress(input, compressed);
    return compressed.str();
}

void check_round_trip(const std::string& original, const std::string& label) {
    std::istringstream encoded(compress_bytes(original), std::ios::binary);
    std::ostringstream restored(std::ios::binary);
    huffman::decompress(encoded, restored);
    if (restored.str() != original) {
        throw std::runtime_error("Round-trip failed for " + label);
    }
}

void check_rejected(const std::string& data, const std::string& label) {
    std::istringstream input(data, std::ios::binary);
    std::ostringstream output(std::ios::binary);
    try {
        huffman::decompress(input, output);
    } catch (const std::runtime_error&) {
        return;
    }
    throw std::runtime_error("Invalid input was accepted: " + label);
}

} // namespace

int main() {
    try {
        check_round_trip("", "empty input");
        check_round_trip(std::string(4096, 'x'), "single-symbol input");
        check_round_trip("Huffman coding is a prefix-code algorithm.", "text input");

        std::string all_bytes;
        for (unsigned int value = 0; value < 256; ++value) {
            all_bytes.push_back(static_cast<char>(value));
        }
        check_round_trip(all_bytes, "all byte values");

        std::string varied;
        for (unsigned int index = 0; index < 10000; ++index) {
            varied.push_back(static_cast<char>((index * 37U + index / 7U) % 256U));
        }
        check_round_trip(varied, "varied binary input");

        check_rejected("bad!", "invalid magic");
        const std::string encoded = compress_bytes("abc");
        std::string truncated = encoded;
        truncated.pop_back();
        check_rejected(truncated, "truncated payload");

        std::string trailing = encoded;
        trailing.push_back('\0');
        check_rejected(trailing, "trailing payload data");

        std::string invalid_padding = encoded;
        invalid_padding.back() |= 1;
        check_rejected(invalid_padding, "nonzero padding bits");

        std::string single_symbol_trailing = compress_bytes("xxxx");
        single_symbol_trailing.push_back('\0');
        check_rejected(single_symbol_trailing, "trailing data after single-symbol file");

        std::cout << "All Huffman tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Test failure: " << error.what() << '\n';
        return 1;
    }
}

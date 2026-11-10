#include "huffman.hpp"

#include <array>
#include <cstdint>
#include <istream>
#include <limits>
#include <memory>
#include <ostream>
#include <queue>
#include <stdexcept>
#include <string>
#include <vector>

namespace huffman {
namespace {

using Frequencies = std::array<std::uint64_t, 256>;

struct Node {
    std::uint64_t frequency;
    std::uint16_t min_symbol;
    int symbol;
    Node* left = nullptr;
    Node* right = nullptr;
    std::uint64_t order;
};

struct NodeOrder {
    bool operator()(const Node* lhs, const Node* rhs) const {
        if (lhs->frequency != rhs->frequency) {
            return lhs->frequency > rhs->frequency;
        }
        if (lhs->min_symbol != rhs->min_symbol) {
            return lhs->min_symbol > rhs->min_symbol;
        }
        return lhs->order > rhs->order;
    }
};

Node* build_tree(const Frequencies& frequencies,
                 std::vector<std::unique_ptr<Node>>& storage) {
    std::priority_queue<Node*, std::vector<Node*>, NodeOrder> queue;
    std::uint64_t order = 0;
    for (std::size_t symbol = 0; symbol < frequencies.size(); ++symbol) {
        if (frequencies[symbol] == 0) {
            continue;
        }
        storage.push_back(std::make_unique<Node>(Node{
            frequencies[symbol], static_cast<std::uint16_t>(symbol),
            static_cast<int>(symbol), nullptr, nullptr, order++}));
        queue.push(storage.back().get());
    }

    while (queue.size() > 1) {
        Node* left = queue.top();
        queue.pop();
        Node* right = queue.top();
        queue.pop();
        storage.push_back(std::make_unique<Node>(Node{
            left->frequency + right->frequency,
            left->min_symbol < right->min_symbol ? left->min_symbol : right->min_symbol,
            -1, left, right, order++}));
        queue.push(storage.back().get());
    }
    return queue.empty() ? nullptr : queue.top();
}

void write_byte(std::ostream& output, std::uint8_t value) {
    output.put(static_cast<char>(value));
    if (!output) {
        throw std::runtime_error("Failed to write compressed data.");
    }
}

std::uint8_t read_byte(std::istream& input) {
    const int value = input.get();
    if (value == std::char_traits<char>::eof()) {
        throw std::runtime_error("Compressed file is truncated.");
    }
    return static_cast<std::uint8_t>(static_cast<unsigned char>(value));
}

void write_u16(std::ostream& output, std::uint16_t value) {
    write_byte(output, static_cast<std::uint8_t>(value & 0xffU));
    write_byte(output, static_cast<std::uint8_t>(value >> 8U));
}

void write_u64(std::ostream& output, std::uint64_t value) {
    for (unsigned int shift = 0; shift < 64; shift += 8) {
        write_byte(output, static_cast<std::uint8_t>((value >> shift) & 0xffU));
    }
}

std::uint16_t read_u16(std::istream& input) {
    const std::uint16_t low = read_byte(input);
    const std::uint16_t high = read_byte(input);
    return static_cast<std::uint16_t>(low | (high << 8U));
}

std::uint64_t read_u64(std::istream& input) {
    std::uint64_t value = 0;
    for (unsigned int shift = 0; shift < 64; shift += 8) {
        value |= static_cast<std::uint64_t>(read_byte(input)) << shift;
    }
    return value;
}

void write_codes(const Node* node, std::string& path,
                 std::array<std::string, 256>& codes) {
    if (node->symbol >= 0) {
        codes[static_cast<std::size_t>(node->symbol)] = path;
        return;
    }
    path.push_back('0');
    write_codes(node->left, path, codes);
    path.back() = '1';
    write_codes(node->right, path, codes);
    path.pop_back();
}

class BitWriter {
public:
    explicit BitWriter(std::ostream& output) : output_(output) {}

    void write(const std::string& bits) {
        for (const char bit : bits) {
            current_ = static_cast<std::uint8_t>((current_ << 1U) | (bit == '1' ? 1U : 0U));
            ++used_;
            if (used_ == 8) {
                write_byte(output_, current_);
                current_ = 0;
                used_ = 0;
            }
        }
    }

    void finish() {
        if (used_ != 0) {
            current_ = static_cast<std::uint8_t>(current_ << (8U - used_));
            write_byte(output_, current_);
        }
    }

private:
    std::ostream& output_;
    std::uint8_t current_ = 0;
    unsigned int used_ = 0;
};

class BitReader {
public:
    explicit BitReader(std::istream& input) : input_(input) {}

    bool read() {
        if (remaining_ == 0) {
            current_ = read_byte(input_);
            remaining_ = 8;
        }
        --remaining_;
        return ((current_ >> remaining_) & 1U) != 0;
    }

    void finish() {
        if (remaining_ != 0) {
            const auto padding_mask = static_cast<std::uint8_t>((1U << remaining_) - 1U);
            if ((current_ & padding_mask) != 0) {
                throw std::runtime_error("Compressed file has invalid padding bits.");
            }
        }
        if (input_.peek() != std::char_traits<char>::eof()) {
            throw std::runtime_error("Compressed file contains unexpected trailing data.");
        }
    }

private:
    std::istream& input_;
    std::uint8_t current_ = 0;
    unsigned int remaining_ = 0;
};

} // namespace

void compress(std::istream& input, std::ostream& output) {
    const auto start = input.tellg();
    if (start == std::istream::pos_type(-1)) {
        throw std::runtime_error("Input must be a seekable stream.");
    }

    Frequencies frequencies{};
    std::uint64_t original_size = 0;
    char buffer[8192];
    while (input.read(buffer, sizeof(buffer)) || input.gcount() > 0) {
        const auto count = static_cast<std::size_t>(input.gcount());
        for (std::size_t index = 0; index < count; ++index) {
            const auto symbol = static_cast<std::uint8_t>(
                static_cast<unsigned char>(buffer[index]));
            if (frequencies[symbol] == std::numeric_limits<std::uint64_t>::max() ||
                original_size == std::numeric_limits<std::uint64_t>::max()) {
                throw std::runtime_error("Input is too large to represent in the file format.");
            }
            ++frequencies[symbol];
            ++original_size;
        }
    }
    if (input.bad()) {
        throw std::runtime_error("Failed to read input file.");
    }

    input.clear();
    input.seekg(start);
    if (!input) {
        throw std::runtime_error("Unable to rewind input stream.");
    }

    std::uint16_t symbol_count = 0;
    for (const auto frequency : frequencies) {
        if (frequency != 0) {
            ++symbol_count;
        }
    }

    output.write("HUF1", 4);
    if (!output) {
        throw std::runtime_error("Failed to write compressed data.");
    }
    write_u64(output, original_size);
    write_u16(output, symbol_count);
    for (std::size_t symbol = 0; symbol < frequencies.size(); ++symbol) {
        if (frequencies[symbol] != 0) {
            write_byte(output, static_cast<std::uint8_t>(symbol));
            write_u64(output, frequencies[symbol]);
        }
    }

    std::vector<std::unique_ptr<Node>> storage;
    const Node* root = build_tree(frequencies, storage);
    if (root == nullptr) {
        return;
    }

    std::array<std::string, 256> codes;
    std::string path;
    write_codes(root, path, codes);
    BitWriter bit_writer(output);
    while (input.read(buffer, sizeof(buffer)) || input.gcount() > 0) {
        const auto count = static_cast<std::size_t>(input.gcount());
        for (std::size_t index = 0; index < count; ++index) {
            const auto symbol = static_cast<std::uint8_t>(
                static_cast<unsigned char>(buffer[index]));
            bit_writer.write(codes[symbol]);
        }
    }
    if (input.bad()) {
        throw std::runtime_error("Failed to read input file.");
    }
    bit_writer.finish();
}

void decompress(std::istream& input, std::ostream& output) {
    char magic[4];
    input.read(magic, sizeof(magic));
    if (input.gcount() != static_cast<std::streamsize>(sizeof(magic)) ||
        std::string(magic, sizeof(magic)) != "HUF1") {
        throw std::runtime_error("Not a valid HUF1 compressed file.");
    }

    const std::uint64_t original_size = read_u64(input);
    const std::uint16_t symbol_count = read_u16(input);
    if (symbol_count > 256 || ((original_size == 0) != (symbol_count == 0))) {
        throw std::runtime_error("Compressed file has an invalid header.");
    }

    Frequencies frequencies{};
    std::uint64_t total_frequency = 0;
    for (std::uint16_t index = 0; index < symbol_count; ++index) {
        const auto symbol = read_byte(input);
        const auto frequency = read_u64(input);
        if (frequency == 0 || frequencies[symbol] != 0 ||
            frequency > std::numeric_limits<std::uint64_t>::max() - total_frequency) {
            throw std::runtime_error("Compressed file has an invalid frequency table.");
        }
        frequencies[symbol] = frequency;
        total_frequency += frequency;
    }
    if (total_frequency != original_size) {
        throw std::runtime_error("Compressed file has an inconsistent frequency table.");
    }

    std::vector<std::unique_ptr<Node>> storage;
    const Node* root = build_tree(frequencies, storage);
    if (root == nullptr) {
        if (input.peek() != std::char_traits<char>::eof()) {
            throw std::runtime_error("Compressed file contains unexpected trailing data.");
        }
        return;
    }

    Frequencies decoded{};
    BitReader bit_reader(input);
    if (root->symbol >= 0) {
        const auto symbol = static_cast<std::uint8_t>(root->symbol);
        for (std::uint64_t index = 0; index < original_size; ++index) {
            write_byte(output, symbol);
        }
        if (input.peek() != std::char_traits<char>::eof()) {
            throw std::runtime_error("Compressed file contains unexpected trailing data.");
        }
        decoded[symbol] = original_size;
    } else {
        for (std::uint64_t index = 0; index < original_size; ++index) {
            const Node* node = root;
            while (node->symbol < 0) {
                node = bit_reader.read() ? node->right : node->left;
            }
            const auto symbol = static_cast<std::uint8_t>(node->symbol);
            if (decoded[symbol] == frequencies[symbol]) {
                throw std::runtime_error("Compressed data does not match its frequency table.");
            }
            ++decoded[symbol];
            write_byte(output, symbol);
        }
        bit_reader.finish();
    }
    if (decoded != frequencies) {
        throw std::runtime_error("Compressed data does not match its frequency table.");
    }
}

} // namespace huffman

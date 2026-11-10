#pragma once

#include <iosfwd>

namespace huffman {

void compress(std::istream& input, std::ostream& output);
void decompress(std::istream& input, std::ostream& output);

} // namespace huffman

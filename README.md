# Huff — File Compressor

A small, lossless file-compression project built around the **Huffman coding** algorithm. It includes a portable C++ command-line program, a browser-based file UI, and automated round-trip tests.

## Features

- Builds a Huffman tree with a min-priority queue and assigns prefix-free bit codes.
- Compresses and restores arbitrary binary files (not just text).
- Uses a compact frequency-table header and checks malformed or truncated input.
- Processes C++ input streams in two passes, so the compressor does not need to hold the whole input file in memory.
- Includes an offline browser UI. Files are processed locally and are not uploaded.

## Build (Windows)

Install a C++17 compiler and CMake, then from the project directory run:

```powershell
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

With the included MinGW compiler, the executable is typically at `build\file-compressor.exe`; with Visual Studio, it is typically at `build\Release\file-compressor.exe`.

## Use the C++ program

```powershell
.\build\file-compressor.exe compress .\photo.png
.\build\file-compressor.exe decompress .\photo.png.huf
```

By default, compression writes `photo.png.huf` and decompression writes `photo.png.huf.restored`. Choose an output path explicitly when preferred:

```powershell
.\build\file-compressor.exe compress .\notes.txt .\notes.huf
.\build\file-compressor.exe decompress .\notes.huf .\notes-restored.txt
.\build\file-compressor.exe --help
```

The source file is never overwritten by the command-line tool.

## Use the browser UI

Open [`web/index.html`](web/index.html) in a modern browser, choose **Compress** or **Decompress**, and drop a file or select it. Download the result from the completion panel. This UI uses the same HUF1 format as the C++ program; it runs entirely in the browser. Because browsers load a file into memory, use the C++ program for very large files.

## How the algorithm works

1. Count the occurrences of each of the 256 possible byte values.
2. Put each used byte into a min-priority queue ordered by frequency and symbol.
3. Repeatedly remove the two least-frequent nodes and join them under a new parent until one tree remains.
4. Encode each byte using its root-to-leaf path (`0` for left and `1` for right).
5. To restore, rebuild the same tree from the saved frequencies and follow the encoded bits back to leaves.

For `n` input bytes and `k` distinct byte values, frequency counting and encoding/decoding take **O(n)** time; building the tree takes **O(k log k)** time. The tree and code table use **O(k)** auxiliary space. The C++ compressor reads the source twice (counting, then encoding).

## HUF1 file format

All multibyte integers are unsigned little-endian:

| Field | Size |
|---|---:|
| Magic bytes `HUF1` | 4 bytes |
| Original file size | 8 bytes |
| Number of distinct symbols | 2 bytes |
| Repeated symbol entries, in ascending byte order: symbol + frequency | 1 + 8 bytes each |
| Huffman payload | Remaining bytes; bits are packed most-significant-bit first, with zero padding |

An empty file has zero symbols and no payload. A one-symbol file needs no payload because its symbol and count are already in the header. The fixed header is 14 bytes, plus 9 bytes for each symbol that occurs. Consequently, tiny or high-entropy inputs may not compress; that is normal for a lossless format with metadata.

## Project layout

```text
include/huffman.hpp       Stream-based codec interface
src/huffman.cpp           Huffman tree, HUF1 reader/writer, and validation
src/main.cpp              Command-line interface
tests/test_huffman.cpp    Binary and edge-case round-trip tests
web/index.html            Responsive browser UI
web/app.js                Browser implementation of the compatible HUF1 codec
```

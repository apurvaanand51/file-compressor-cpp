# Huff — Huffman File Compressor

Huff is a small, lossless file-compression project built as a Data Structures and Algorithms demonstration. It uses **Huffman coding** to encode files, and includes a C++ command-line application, an offline browser interface, and tests.

The command-line app and browser UI use the same `HUF1` file format. A file compressed in either interface can be decompressed in the other.

## What it does

- Compresses and restores text and binary files, including images and other file types.
- Builds a Huffman tree using a min-priority queue and encodes symbols with prefix-free bit codes.
- Saves the original size and byte frequencies so the tree can be rebuilt during decompression.
- Detects invalid, truncated, or inconsistent compressed files.
- Processes files locally. The browser interface does not upload files.
- Includes a sample text file and automated round-trip tests.

> **Compression is not guaranteed to make every file smaller.** The compressed file includes a header and a frequency table. For small files or files with many different byte values, this overhead can be larger than the space saved by encoding.

## Requirements

To build the C++ application, install:

- A C++17-compatible compiler:
  - **Windows:** MinGW-w64 (GCC) or Visual Studio with the C++ desktop workload.
  - **macOS:** Xcode Command Line Tools (Clang).
  - **Linux:** GCC or Clang and the standard C++ development tools.
- **CMake 3.16 or newer.**

There are no third-party libraries or package downloads required by this project.

To use the browser interface, you only need a modern browser. No C++ compiler is needed if you only want to use the browser UI.

## Get started on Windows

Open PowerShell in the project folder (the folder containing `CMakeLists.txt`). For example:

```powershell
cd "C:\path\to\Project"
```

### 1. Configure and build

If CMake can find your compiler automatically:

```powershell
cmake -S . -B build
cmake --build build --config Release
```

If you installed MinGW-w64 but CMake does not select it automatically, specify the MinGW Makefiles generator:

```powershell
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build
```

If you use Visual Studio, you can select an installed version explicitly (replace the example version with one listed by `cmake --help`):

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

The executable is usually `build\file-compressor.exe` with MinGW or another single-configuration generator, and `build\Release\file-compressor.exe` with Visual Studio. Use the path matching the generator you selected.

### 2. Run the tests (optional)

```powershell
ctest --test-dir build -C Release --output-on-failure
```

With MinGW, you can also run the test command without the configuration option:

```powershell
ctest --test-dir build --output-on-failure
```

### 3. Try the browser interface

You can run the browser UI either directly from the file or from a small local web server:

Option A: open the page directly in your browser

```powershell
Start-Process "web\index.html"
```

Option B: serve the project from a local web server and open the page in a browser

```powershell
cd "C:\path\to\Project"
python -m http.server 8000
```

Then open:

```text
http://localhost:8000/web/index.html
```

Once the page loads, select **Compress** or **Decompress**, then choose a file or drag it into the drop area. Use the download link that appears after processing finishes.

For a quick test, select the included `sample.txt`. Compress it, download the `.huf` result, switch to **Decompress**, and choose that downloaded file. The restored file should contain the original text.

The browser reads the selected file into memory. For very large files, use the C++ command-line program instead.

## Command-line usage

The program takes a command, an input path, and an optional output path:

```text
file-compressor compress <input-file> [output-file]
file-compressor decompress <input-file> [output-file]
file-compressor --help
```

In PowerShell, with a MinGW build:

```powershell
.\build\file-compressor.exe compress .\sample.txt
.\build\file-compressor.exe decompress .\sample.txt.huf
```

With a Visual Studio Release build, use:

```powershell
.\build\Release\file-compressor.exe compress .\sample.txt
```

By default, compression writes a new file next to the input with `.huf` appended to its name. Decompression writes a new file with `.restored` appended. To choose the output filename yourself:

```powershell
.\build\file-compressor.exe compress .\photo.png .\photo.png.huf
.\build\file-compressor.exe decompress .\photo.png.huf .\photo-restored.png
```

The command-line program refuses to use the same file as both input and output, so the source file is not overwritten.

### macOS and Linux

From a terminal in the project directory, configure, build, and test:

```sh
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build --output-on-failure
```

Run the resulting executable (on macOS and Linux it has no `.exe` extension):

```sh
./build/file-compressor compress ./sample.txt
./build/file-compressor decompress ./sample.txt.huf
./build/file-compressor --help
```

## How Huffman coding works

1. Count how often each of the 256 possible byte values appears in the input.
2. Add each byte that occurs to a min-priority queue, ordered by frequency.
3. Repeatedly remove the two least-frequent nodes and join them under a new parent node until one tree remains.
4. Assign `0` to each left edge and `1` to each right edge. A byte's code is the path from the tree root to its leaf.
5. To decompress, rebuild the tree from the saved frequencies and follow each payload bit from the root to a leaf.

Because each byte is stored at a leaf, no byte's code is a prefix of another's. This property makes the encoded bit stream unambiguous.

For an input of `n` bytes and `k` distinct byte values, counting and encoding/decoding take **O(n)** time; building the tree takes **O(k log k)** time. Tree construction and code storage use **O(k)** auxiliary space. The C++ compressor reads the input twice: once to count frequencies and once to encode it, so it does not need to hold the entire input file in memory.

## HUF1 file format

Multibyte numbers are unsigned and stored in little-endian byte order.

| Field | Size |
|---|---:|
| Magic identifier (`HUF1`) | 4 bytes |
| Original file size | 8 bytes |
| Number of distinct byte values | 2 bytes |
| One entry per distinct value: byte value and its frequency | 9 bytes per value |
| Encoded Huffman payload | Remaining bytes |

Payload bits are packed most-significant-bit first. Unused bits in the last payload byte are zero. An empty input has no symbols or payload. An input containing just one distinct byte value needs no payload: its value and repetition count are already in the header.

The fixed header is 14 bytes, and each distinct byte value adds another 9 bytes. This metadata explains why small or high-entropy files can become larger when compressed.

## Project files

```text
CMakeLists.txt         CMake build configuration
include/huffman.hpp    Public stream-based compression interface
src/huffman.cpp        Huffman tree and HUF1 compression/decompression
src/main.cpp           Command-line interface
tests/test_huffman.cpp Round-trip and invalid-file tests
web/index.html         Browser interface
web/app.js             Browser implementation of the HUF1 codec
sample.txt             Sample file for trying compression
```

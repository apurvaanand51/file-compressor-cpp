"use strict";

const MAGIC = [0x48, 0x55, 0x46, 0x31];
const input = document.getElementById("file-input");
const dropZone = document.getElementById("drop-zone");
const action = document.getElementById("action");
const selectedFile = document.getElementById("selected-file");
const errorBox = document.getElementById("error");
const resultBox = document.getElementById("result");
let mode = "compress";
let file = null;
let downloadUrl = null;

function formatBytes(bytes) {
  if (bytes < 1024) return `${bytes} B`;
  const units = ["KB", "MB", "GB", "TB"];
  let value = bytes;
  let unit = -1;
  do {
    value /= 1024;
    unit += 1;
  } while (value >= 1024 && unit < units.length - 1);
  return `${value.toFixed(value < 10 ? 1 : 0)} ${units[unit]}`;
}

function setMode(nextMode) {
  mode = nextMode;
  document.getElementById("compress-tab").setAttribute("aria-selected", String(mode === "compress"));
  document.getElementById("decompress-tab").setAttribute("aria-selected", String(mode === "decompress"));
  document.getElementById("drop-title").textContent =
    mode === "compress" ? "Drop a file here to compress" : "Drop a .huf file here to restore";
  input.accept = mode === "decompress" ? ".huf,application/octet-stream" : "";
  clearMessages();
  refreshFile();
}

function clearMessages() {
  errorBox.textContent = "";
  errorBox.classList.remove("visible");
  resultBox.classList.remove("visible");
  if (downloadUrl !== null) {
    URL.revokeObjectURL(downloadUrl);
    downloadUrl = null;
  }
}

function refreshFile() {
  selectedFile.classList.toggle("visible", file !== null);
  if (file !== null) {
    document.getElementById("file-name").textContent = file.name;
    document.getElementById("file-size").textContent = formatBytes(file.size);
  }
  action.disabled = file === null;
  action.textContent = file === null
    ? "Choose a file to begin"
    : mode === "compress" ? "Compress file" : "Decompress file";
}

function selectFile(nextFile) {
  file = nextFile;
  clearMessages();
  refreshFile();
}

function compareNodes(left, right) {
  if (left.frequency !== right.frequency) return left.frequency < right.frequency ? -1 : 1;
  if (left.minSymbol !== right.minSymbol) return left.minSymbol - right.minSymbol;
  return left.order - right.order;
}

function buildTree(frequencies) {
  const queue = [];
  let order = 0;
  frequencies.forEach((frequency, symbol) => {
    if (frequency !== 0n) {
      queue.push({ frequency, minSymbol: symbol, symbol, left: null, right: null, order: order++ });
    }
  });
  while (queue.length > 1) {
    queue.sort(compareNodes);
    const left = queue.shift();
    const right = queue.shift();
    queue.push({
      frequency: left.frequency + right.frequency,
      minSymbol: Math.min(left.minSymbol, right.minSymbol),
      symbol: null,
      left,
      right,
      order: order++
    });
  }
  return queue.length === 0 ? null : queue[0];
}

function createCodes(root) {
  const codes = new Array(256).fill("");
  function visit(node, path) {
    if (node.symbol !== null) {
      codes[node.symbol] = path;
      return;
    }
    visit(node.left, `${path}0`);
    visit(node.right, `${path}1`);
  }
  visit(root, "");
  return codes;
}

function compressBytes(bytes) {
  const frequencies = new Array(256).fill(0n);
  for (const byte of bytes) frequencies[byte] += 1n;
  const symbolCount = frequencies.reduce((count, frequency) => count + (frequency === 0n ? 0 : 1), 0);
  const header = new Uint8Array(14 + symbolCount * 9);
  const view = new DataView(header.buffer);
  header.set(MAGIC, 0);
  view.setBigUint64(4, BigInt(bytes.length), true);
  view.setUint16(12, symbolCount, true);

  let offset = 14;
  for (let symbol = 0; symbol < 256; symbol += 1) {
    if (frequencies[symbol] !== 0n) {
      header[offset] = symbol;
      view.setBigUint64(offset + 1, frequencies[symbol], true);
      offset += 9;
    }
  }

  if (bytes.length === 0) return header;
  const root = buildTree(frequencies);
  const codes = createCodes(root);
  const encoded = [];
  let current = 0;
  let used = 0;
  for (const byte of bytes) {
    for (const bit of codes[byte]) {
      current = (current << 1) | (bit === "1" ? 1 : 0);
      used += 1;
      if (used === 8) {
        encoded.push(current);
        current = 0;
        used = 0;
      }
    }
  }
  if (used !== 0) encoded.push(current << (8 - used));

  const result = new Uint8Array(header.length + encoded.length);
  result.set(header);
  result.set(encoded, header.length);
  return result;
}

function decompressBytes(bytes) {
  if (bytes.length < 14 || !MAGIC.every((value, index) => bytes[index] === value)) {
    throw new Error("This is not a valid HUF1 compressed file.");
  }
  const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  const originalSize = view.getBigUint64(4, true);
  const symbolCount = view.getUint16(12, true);
  if (symbolCount > 256 || ((originalSize === 0n) !== (symbolCount === 0))) {
    throw new Error("The compressed file has an invalid header.");
  }

  const frequencies = new Array(256).fill(0n);
  let offset = 14;
  let total = 0n;
  for (let index = 0; index < symbolCount; index += 1) {
    if (offset + 9 > bytes.length) throw new Error("The compressed file is truncated.");
    const symbol = bytes[offset];
    const frequency = view.getBigUint64(offset + 1, true);
    if (frequency === 0n || frequencies[symbol] !== 0n) {
      throw new Error("The compressed file has an invalid frequency table.");
    }
    frequencies[symbol] = frequency;
    total += frequency;
    offset += 9;
  }
  if (total !== originalSize) throw new Error("The compressed file has an inconsistent frequency table.");
  if (originalSize > BigInt(Number.MAX_SAFE_INTEGER)) {
    throw new Error("This file is too large for this browser to restore safely.");
  }

  const root = buildTree(frequencies);
  const output = new Uint8Array(Number(originalSize));
  if (root === null) {
    if (offset !== bytes.length) throw new Error("The compressed file contains unexpected trailing data.");
    return output;
  }

  const decoded = new Array(256).fill(0n);
  if (root.symbol !== null) {
    if (offset !== bytes.length) throw new Error("The compressed file contains unexpected trailing data.");
    output.fill(root.symbol);
    decoded[root.symbol] = originalSize;
  } else {
    let bitPosition = offset * 8;
    for (let index = 0; index < output.length; index += 1) {
      let node = root;
      while (node.symbol === null) {
        if (bitPosition >= bytes.length * 8) throw new Error("The compressed file is truncated.");
        const bit = (bytes[bitPosition >> 3] >> (7 - (bitPosition & 7))) & 1;
        bitPosition += 1;
        node = bit === 1 ? node.right : node.left;
      }
      if (decoded[node.symbol] === frequencies[node.symbol]) {
        throw new Error("Compressed data does not match its frequency table.");
      }
      decoded[node.symbol] += 1n;
      output[index] = node.symbol;
    }
    for (let symbol = 0; symbol < 256; symbol += 1) {
      if (decoded[symbol] !== frequencies[symbol]) {
        throw new Error("Compressed data does not match its frequency table.");
      }
    }
    const usedBits = bitPosition - offset * 8;
    const byteCount = Math.ceil(usedBits / 8);
    if (offset + byteCount !== bytes.length) {
      throw new Error("The compressed file contains unexpected trailing data.");
    }
    const remainder = usedBits & 7;
    if (remainder !== 0) {
      const paddingMask = (1 << (8 - remainder)) - 1;
      if ((bytes[bytes.length - 1] & paddingMask) !== 0) {
        throw new Error("The compressed file has invalid padding bits.");
      }
    }
  }
  return output;
}

function showResult(output, outputName, originalSize, resultSize) {
  if (downloadUrl !== null) URL.revokeObjectURL(downloadUrl);
  downloadUrl = URL.createObjectURL(new Blob([output], { type: "application/octet-stream" }));
  const link = document.getElementById("download");
  link.href = downloadUrl;
  link.download = outputName;
  link.textContent = `Download ${outputName} ↓`;
  document.getElementById("result-title").textContent =
    mode === "compress" ? "Compression complete" : "File restored successfully";
  let detail;
  if (mode === "compress") {
    if (originalSize === 0) {
      detail = `${formatBytes(originalSize)} → ${formatBytes(resultSize)} (empty input; HUF1 header)`;
    } else if (resultSize <= originalSize) {
      detail = `${formatBytes(originalSize)} → ${formatBytes(resultSize)} (${(100 * (1 - resultSize / originalSize)).toFixed(1)}% size reduction)`;
    } else {
      detail = `${formatBytes(originalSize)} → ${formatBytes(resultSize)} (file grew by ${(100 * (resultSize / originalSize - 1)).toFixed(1)}%; header overhead)`;
    }
  } else {
    detail = `${formatBytes(resultSize)} restored from ${formatBytes(originalSize)} compressed`;
  }
  document.getElementById("result-detail").textContent = detail;
  resultBox.classList.add("visible");
}

document.getElementById("compress-tab").addEventListener("click", () => setMode("compress"));
document.getElementById("decompress-tab").addEventListener("click", () => setMode("decompress"));
input.addEventListener("change", () => selectFile(input.files[0] || null));

for (const eventName of ["dragenter", "dragover"]) {
  dropZone.addEventListener(eventName, (event) => {
    event.preventDefault();
    dropZone.classList.add("dragging");
  });
}
for (const eventName of ["dragleave", "drop"]) {
  dropZone.addEventListener(eventName, (event) => {
    event.preventDefault();
    dropZone.classList.remove("dragging");
  });
}
dropZone.addEventListener("drop", (event) => {
  const droppedFile = event.dataTransfer.files[0];
  if (droppedFile) selectFile(droppedFile);
});

action.addEventListener("click", async () => {
  if (file === null) return;
  clearMessages();
  action.disabled = true;
  action.textContent = mode === "compress" ? "Compressing…" : "Restoring…";
  try {
    const bytes = new Uint8Array(await file.arrayBuffer());
    const output = mode === "compress" ? compressBytes(bytes) : decompressBytes(bytes);
    const outputName = mode === "compress"
      ? `${file.name}.huf`
      : file.name.toLowerCase().endsWith(".huf") ? file.name.slice(0, -4) || "restored-file" : `${file.name}.restored`;
    showResult(output, outputName, file.size, output.length);
  } catch (error) {
    errorBox.textContent = error instanceof Error ? error.message : "The file could not be processed.";
    errorBox.classList.add("visible");
  } finally {
    refreshFile();
  }
});

refreshFile();

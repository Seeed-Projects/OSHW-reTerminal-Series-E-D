const PARTITION_SIZE = 0x5000;
const PAGE_SIZE = 4096;
const PAGE_COUNT = PARTITION_SIZE / PAGE_SIZE;
const PAGE_HEADER_SIZE = 32;
const ENTRY_STATE_SIZE = 32;
const ENTRY_SIZE = 32;
const ENTRY_COUNT = 126;
const ENTRY_BASE = PAGE_HEADER_SIZE + ENTRY_STATE_SIZE;
const PAGE_STATE_ACTIVE = 0xfffffffe;
const PAGE_VERSION = 0xfe;
const ENTRY_STATE_WRITTEN = 0b10;
const TYPE_U8 = 0x01;
const TYPE_I32 = 0x14;
const TYPE_STR = 0x21;
const TYPE_BLOB_DATA = 0x42;
const TYPE_BLOB_IDX = 0x48;
const BLOB_CHUNK_VER_OFFSET = 0x00;
const NAMESPACE_INDEX = 1;
const NAMESPACE_NAME = "config";

const crcTable = new Uint32Array(256);
for (let i = 0; i < crcTable.length; i++) {
  let value = i;
  for (let bit = 0; bit < 8; bit++) {
    value = (value & 1) ? (value >>> 1) ^ 0xedb88320 : value >>> 1;
  }
  crcTable[i] = value >>> 0;
}

function crc32(bytes) {
  // ESP-IDF NVS uses the ESP32 ROM crc32_le, which inverts the register at
  // both ends. Chained from 0xffffffff this nets to: start register 0x00000000,
  // run the reflected table loop, then XOR the result with 0xffffffff.
  // ESP-IDF NVS 使用 ESP32 ROM 的 crc32_le：首尾各取反一次。
  // 从 0xffffffff 链式调用，净效果是：寄存器从 0 开始，跑反射表循环，最后再与 0xffffffff 异或。
  let crc = 0x00000000;
  for (const byte of bytes) {
    crc = (crc >>> 8) ^ crcTable[(crc ^ byte) & 0xff];
  }
  return (crc ^ 0xffffffff) >>> 0;
}

function writeUint32LE(data, offset, value) {
  new DataView(data.buffer, data.byteOffset, data.byteLength).setUint32(
    offset,
    value >>> 0,
    true
  );
}

function setEntryState(page, entryIndex) {
  const bitOffset = entryIndex * 2;
  const byteOffset = PAGE_HEADER_SIZE + Math.floor(bitOffset / 8);
  const shift = bitOffset % 8;
  page[byteOffset] =
    (page[byteOffset] & ~(0b11 << shift)) | (ENTRY_STATE_WRITTEN << shift);
}

function writeKey(entry, key) {
  if (typeof key !== "string" || !key.length) {
    throw new Error("NVS key must be a non-empty string");
  }
  const keyBytes = new TextEncoder().encode(key);
  if (keyBytes.length > 15) {
    throw new Error(`NVS key is too long: ${key}`);
  }
  entry.fill(0x00, 8, 24);
  entry.set(keyBytes, 8);
}

function makeEntry(nsIndex, type, span, chunkIndex, key, dataBytes) {
  const entry = new Uint8Array(ENTRY_SIZE);
  entry.fill(0xff);
  entry[0] = nsIndex;
  entry[1] = type;
  entry[2] = span;
  entry[3] = chunkIndex;
  writeKey(entry, key);
  entry.set(dataBytes, 24);

  const crcInput = new Uint8Array(28);
  crcInput.set(entry.subarray(0, 4), 0);
  crcInput.set(entry.subarray(8, 32), 4);
  writeUint32LE(entry, 4, crc32(crcInput));
  return entry;
}

function normalizeBlob(value) {
  if (value instanceof Uint8Array) return value;
  if (value instanceof ArrayBuffer) return new Uint8Array(value);
  if (ArrayBuffer.isView(value)) {
    return new Uint8Array(value.buffer, value.byteOffset, value.byteLength);
  }
  if (Array.isArray(value)) return new Uint8Array(value);
  throw new Error("NVS blob value must be byte-like data");
}

// Splits a payload into 32-byte entry-sized chunks padded with 0xFF.
// 把 payload 切成 32 字节一个的 entry chunk，不足处用 0xFF 补齐。
function splitPayloadChunks(payload) {
  const chunks = [];
  for (let offset = 0; offset < payload.length; offset += ENTRY_SIZE) {
    const chunk = new Uint8Array(ENTRY_SIZE);
    chunk.fill(0xff);
    chunk.set(payload.subarray(offset, offset + ENTRY_SIZE), 0);
    chunks.push(chunk);
  }
  return chunks;
}

// Builds the variable-length descriptor: size(2) + reserved(2, 0xFFFF) + dataCrc32(4).
// 构建变长描述符：大小(2) + 保留(2，0xFFFF) + 数据 CRC32(4)。
function makeVarLenDescriptor(payload) {
  const descriptor = new Uint8Array(8);
  descriptor.fill(0xff);
  new DataView(descriptor.buffer).setUint16(0, payload.length, true);
  writeUint32LE(descriptor, 4, crc32(payload));
  return descriptor;
}

// Builds the ordered list of 32-byte entries representing one config value.
// 构建表示一个配置项的、按写入顺序排列的 32 字节 entry 列表。
function makeDataEntry(key, type, value) {
  const data = new Uint8Array(8);
  data.fill(0xff);

  if (type === "u8") {
    data[0] = Number(value) & 0xff;
    return [makeEntry(NAMESPACE_INDEX, TYPE_U8, 1, 0xff, key, data)];
  }

  if (type === "i32") {
    new DataView(data.buffer).setInt32(0, Number(value), true);
    return [makeEntry(NAMESPACE_INDEX, TYPE_I32, 1, 0xff, key, data)];
  }

  if (type === "string") {
    const encoded = new TextEncoder().encode(String(value));
    const payload = new Uint8Array(encoded.length + 1);
    payload.set(encoded, 0);
    const span = 1 + Math.ceil(payload.length / ENTRY_SIZE);
    return [
      makeEntry(NAMESPACE_INDEX, TYPE_STR, span, 0xff, key, makeVarLenDescriptor(payload)),
      ...splitPayloadChunks(payload),
    ];
  }

  if (type === "blob") {
    // ESP-IDF V2 multipage blob: a BLOB_DATA group (descriptor + payload chunks)
    // is written first, then a BLOB_IDX entry recording total size and chunk
    // count. The firmware reads this through getBytes/getFloat.
    // ESP-IDF V2 多页 blob：先写 BLOB_DATA 组（描述符 + payload chunk），
    // 再写 BLOB_IDX entry（记录总大小与 chunk 数）。固件通过 getBytes/getFloat 读取。
    const payload = normalizeBlob(value);
    const chunkCount = Math.ceil(payload.length / ENTRY_SIZE);
    const dataSpan = 1 + chunkCount;

    const dataEntry = makeEntry(
      NAMESPACE_INDEX, TYPE_BLOB_DATA, dataSpan, BLOB_CHUNK_VER_OFFSET, key,
      makeVarLenDescriptor(payload)
    );

    const idxData = new Uint8Array(8);
    idxData.fill(0xff);
    writeUint32LE(idxData, 0, payload.length);
    idxData[4] = chunkCount;
    idxData[5] = BLOB_CHUNK_VER_OFFSET;
    const idxEntry = makeEntry(NAMESPACE_INDEX, TYPE_BLOB_IDX, 1, 0xff, key, idxData);

    return [dataEntry, ...splitPayloadChunks(payload), idxEntry];
  }

  throw new Error(`Unsupported NVS type: ${type}`);
}

function writeEntry(page, entryIndex, entry) {
  if (entryIndex >= ENTRY_COUNT) {
    throw new Error("NVS page does not have enough free entries");
  }
  page.set(entry, ENTRY_BASE + entryIndex * ENTRY_SIZE);
  setEntryState(page, entryIndex);
}

function initActivePage(partition) {
  const page = partition.subarray(0, PAGE_SIZE);
  writeUint32LE(page, 0, PAGE_STATE_ACTIVE);
  writeUint32LE(page, 4, 0);
  page[8] = PAGE_VERSION;
  page.fill(0xff, 9, 28);
  writeUint32LE(page, 28, crc32(page.subarray(4, 28)));
  return page;
}

export const NVS_VERSION = 3;

export function generateNvsPartition(entries) {
  if (!Array.isArray(entries)) {
    throw new Error("NVS entries must be an array");
  }

  const partition = new Uint8Array(PARTITION_SIZE);
  partition.fill(0xff);
  const page = initActivePage(partition);

  let entryIndex = 0;
  const namespaceData = new Uint8Array([1, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff]);
  writeEntry(page, entryIndex++, makeEntry(0, TYPE_U8, 1, 0xff, NAMESPACE_NAME, namespaceData));

  for (const item of entries) {
    const { key, type, value } = item || {};
    const records = makeDataEntry(key, type, value);
    for (const record of records) {
      writeEntry(page, entryIndex++, record);
    }
  }

  if (PAGE_COUNT !== 5) {
    throw new Error("Unexpected NVS partition layout");
  }
  return partition;
}

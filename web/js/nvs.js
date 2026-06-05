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
const TYPE_BLOB = 0x41;
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
  let crc = 0xffffffff;
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

function makeDataEntry(key, type, value) {
  const data = new Uint8Array(8);
  data.fill(0xff);

  if (type === "u8") {
    data[0] = Number(value) & 0xff;
    return { entry: makeEntry(NAMESPACE_INDEX, TYPE_U8, 1, 0xff, key, data), chunks: [] };
  }

  if (type === "i32") {
    new DataView(data.buffer).setInt32(0, Number(value), true);
    return { entry: makeEntry(NAMESPACE_INDEX, TYPE_I32, 1, 0xff, key, data), chunks: [] };
  }

  let payload;
  let typeCode;
  if (type === "string") {
    const encoded = new TextEncoder().encode(String(value));
    payload = new Uint8Array(encoded.length + 1);
    payload.set(encoded, 0);
    typeCode = TYPE_STR;
  } else if (type === "blob") {
    payload = normalizeBlob(value);
    typeCode = TYPE_BLOB;
  } else {
    throw new Error(`Unsupported NVS type: ${type}`);
  }

  const span = 1 + Math.ceil(payload.length / ENTRY_SIZE);
  const chunks = [];
  for (let offset = 0; offset < payload.length; offset += ENTRY_SIZE) {
    const chunk = new Uint8Array(ENTRY_SIZE);
    chunk.fill(0xff);
    chunk.set(payload.subarray(offset, offset + ENTRY_SIZE), 0);
    chunks.push(chunk);
  }

  const descriptor = new Uint8Array(8);
  descriptor.fill(0x00);
  new DataView(descriptor.buffer).setUint16(0, payload.length, true);
  writeUint32LE(descriptor, 4, crc32(payload));
  return {
    entry: makeEntry(NAMESPACE_INDEX, typeCode, span, 0xff, key, descriptor),
    chunks,
  };
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
    const record = makeDataEntry(key, type, value);
    writeEntry(page, entryIndex++, record.entry);
    for (const chunk of record.chunks) {
      writeEntry(page, entryIndex++, chunk);
    }
  }

  if (PAGE_COUNT !== 5) {
    throw new Error("Unexpected NVS partition layout");
  }
  return partition;
}

import { md5 } from "js-md5";
import { hex, sha256, validateArtifact, validateManifest, verifiedRead } from "./guards.js";

export const FLASH_BYTES = 0x1000000;
export const BACKUP_CHUNK_BYTES = 0x40000;
export const BACKUP_TIMEOUT_MS = 8 * 60 * 1000;
const SECTOR_BYTES = 0x1000;
const TABLE_OFFSET = 0x8000;

const layout = rows => Object.freeze(rows.map(([name, type, subtype, offset, size_bytes]) =>
  Object.freeze({ name, type, subtype, offset, size_bytes, flags: 0 })));

// Only nonpersonal partition metadata is recorded here. Verified September 2026
// against the original manufacturer's table: checksum and every padding byte.
// A new factory revision needs its own review; matching some offsets is not enough.
export const APPROVED_FACTORY_SOURCE = Object.freeze({
  id: "m5-stopwatch-factory-layout-1",
  sectorSha256: "551de813efa412911f6d15afef5c0cdb9b3c2d9d8dece864280fa39963117e8f",
  partitions: layout([
    ["nvs", 1, 2, 0x9000, 0x4000],
    ["otadata", 1, 0, 0xd000, 0x2000],
    ["phy_init", 1, 1, 0xf000, 0x1000],
    ["ota_0", 0, 0x10, 0x20000, 0x4f0000],
    ["ota_1", 0, 0x11, 0x510000, 0x4f0000],
    ["storage", 1, 0x81, 0xa00000, 0x400000],
    ["coredump", 1, 3, 0xe00000, 0x10000],
  ]),
});

export const TARGET_SECTOR_SHA256 = "0bcf1787e46f4bf1ce9ad28bd22c2257e715987e913add5008c0265d3feb2fcd";
export const TARGET_PARTITIONS = layout([
  ["nvs", 1, 2, 0x9000, 0x5000],
  ["otadata", 1, 0, 0xe000, 0x2000],
  ["app0", 0, 0x10, 0x10000, 0x300000],
  ["app1", 0, 0x11, 0x310000, 0x300000],
  ["ffat", 1, 0x81, 0x610000, 0x9e0000],
  ["coredump", 1, 3, 0xff0000, 0x10000],
]);
export const FACTORY_ERASED_RANGES = Object.freeze([
  Object.freeze({ address: 0x9000, size: 0x7000, purpose: "Fresh settings and app0 boot selection" }),
  Object.freeze({ address: 0x310000, size: 0xcf0000, purpose: "Unused application, profile storage, and crash data" }),
]);

const verifiedBackups = new WeakSet();
const savedBackups = new WeakSet();
const stop = message => new Error(`${message} Nothing was written.`);
const exactSource = source => {
  if (source !== APPROVED_FACTORY_SOURCE) throw stop("An approved factory layout is required.");
};

// Same record/checksum contract as scripts/check_flash_layout.py, additionally
// requiring a complete sector and aligned, ordered, nonoverlapping entries.
export function parsePartitionSector(bytes) {
  if (!(bytes instanceof Uint8Array) || bytes.length !== SECTOR_BYTES)
    throw stop("Incomplete partition sector.");
  const entries = [];
  const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  let previousEnd = 0x9000;
  for (let offset = 0; offset < bytes.length; offset += 32) {
    const record = bytes.subarray(offset, offset + 32);
    if (record[0] === 0xeb && record[1] === 0xeb) {
      if (!record.subarray(2, 16).every(b => b === 255) ||
          hex(record.subarray(16)) !== md5(bytes.subarray(0, offset)) ||
          !entries.length || !bytes.subarray(offset + 32).every(b => b === 255))
        throw stop("Invalid partition checksum or padding.");
      return entries;
    }
    if (view.getUint16(offset, true) !== 0x50aa) throw stop("Unrecognized partition table.");
    const rawName = record.subarray(12, 28);
    const end = rawName.indexOf(0);
    const label = rawName.subarray(0, end < 0 ? 16 : end);
    if (!label.length || !label.every(b => b >= 32 && b <= 126) ||
        (end >= 0 && !rawName.subarray(end).every(b => b === 0)))
      throw stop("Invalid partition label.");
    const entry = {
      name: String.fromCharCode(...label), type: record[2], subtype: record[3],
      offset: view.getUint32(offset + 4, true), size_bytes: view.getUint32(offset + 8, true),
      flags: view.getUint32(offset + 28, true),
    };
    if (!entry.size_bytes || entry.offset % SECTOR_BYTES || entry.size_bytes % SECTOR_BYTES ||
        entry.offset < previousEnd || entry.offset + entry.size_bytes > FLASH_BYTES)
      throw stop("Invalid partition boundaries.");
    previousEnd = entry.offset + entry.size_bytes;
    entries.push(entry);
  }
  throw stop("Partition checksum is missing.");
}

function verifyLayout(bytes, expected) {
  if (JSON.stringify(parsePartitionSector(bytes)) !== JSON.stringify(expected))
    throw stop("The partition layout is not approved for this operation.");
}

function verifyTargetRelease(release) {
  validateManifest(release);
  if (release.partition_table.sector_sha256 !== TARGET_SECTOR_SHA256 ||
      release.partition_table.padding_byte !== 255 ||
      !Array.isArray(release.partition_table.layout) ||
      release.partition_table.layout.length !== TARGET_PARTITIONS.length ||
      release.partition_table.layout.some((entry, index) =>
        Object.entries(TARGET_PARTITIONS[index]).some(([key, value]) => entry[key] !== value)))
    throw stop("This first-install plan requires the approved conference partition map.");
}

export async function inspectFactorySource(sector, release) {
  verifyTargetRelease(release);
  if (!(sector instanceof Uint8Array) || sector.length !== SECTOR_BYTES)
    throw stop("Incomplete partition sector.");
  const digest = await sha256(sector);
  if (digest === TARGET_SECTOR_SHA256)
    throw stop("This badge is already prepared. Choose Update badge to preserve its settings and profile.");
  if (digest !== APPROVED_FACTORY_SOURCE.sectorSha256)
    throw stop("This factory layout is not recognized. First install cannot safely convert it.");
  verifyLayout(sector, APPROVED_FACTORY_SOURCE.partitions);
  return APPROVED_FACTORY_SOURCE;
}

export async function createFactoryPlan(release, files, source) {
  exactSource(source);
  verifyTargetRelease(release);
  if (!Array.isArray(files) || files.length !== release.artifacts.length)
    throw stop("The verified firmware downloads are incomplete.");
  const fileArray = [];
  for (const artifact of release.artifacts) {
    const matches = files.filter(file => file.address === artifact.offset);
    if (matches.length !== 1 || !(matches[0].data instanceof Uint8Array))
      throw stop("The firmware components do not match the release.");
    const data = matches[0].data.slice();
    await validateArtifact(artifact, data);
    if (artifact.offset === TABLE_OFFSET) {
      const sector = new Uint8Array(SECTOR_BYTES).fill(255);
      sector.set(data);
      verifyLayout(sector, TARGET_PARTITIONS);
      if (await sha256(sector) !== TARGET_SECTOR_SHA256)
        throw stop("The downloaded partition table is not approved.");
      fileArray.push({ address: artifact.offset, data: sector });
    } else fileArray.push({ address: artifact.offset, data });
  }
  // An enormous all-FF image compresses to one packet, leaving esptool waiting
  // for a long uninterrupted erase. Bound each erase/write/MD5 operation. Keep
  // compression enabled: the pinned noncompressed writer pads to larger blocks.
  for (const range of FACTORY_ERASED_RANGES) {
    for (let offset = 0; offset < range.size; offset += BACKUP_CHUNK_BYTES)
      fileArray.push({ address: range.address + offset,
        data: new Uint8Array(Math.min(BACKUP_CHUNK_BYTES, range.size - offset)).fill(255) });
  }
  fileArray.sort((a, b) => a.address - b.address);
  let previousEnd = 0;
  for (const file of fileArray) {
    // esptool erases complete sectors even when an application ends mid-sector.
    const eraseEnd = file.address + Math.ceil(file.data.length / SECTOR_BYTES) * SECTOR_BYTES;
    if (file.address % SECTOR_BYTES || file.address < previousEnd || eraseEnd > FLASH_BYTES)
      throw stop("The first-install ranges overlap or exceed the flash.");
    previousEnd = eraseEnd;
  }
  return {
    fileArray, eraseAll: false,
    sourceSectorSha256: source.sectorSha256,
    targetSectorSha256: TARGET_SECTOR_SHA256,
    erasedRanges: FACTORY_ERASED_RANGES,
  };
}

export async function backupFactoryFlash(loader, transport, source, {
  reportProgress = () => {}, signal, timeoutMs = BACKUP_TIMEOUT_MS, now = () => performance.now(),
} = {}) {
  exactSource(source);
  if (!Number.isFinite(timeoutMs) || timeoutMs <= 0 || timeoutMs > BACKUP_TIMEOUT_MS)
    throw stop("Invalid recovery backup deadline.");
  const deadline = now() + timeoutMs;
  const check = () => {
    if (signal?.aborted) throw stop("Recovery backup cancelled.");
    if (now() >= deadline) throw stop("Recovery backup timed out. Reconnect the badge and try again.");
  };
  check();
  const bytes = new Uint8Array(FLASH_BYTES);
  reportProgress(0);
  for (let offset = 0; offset < FLASH_BYTES; offset += BACKUP_CHUNK_BYTES) {
    check();
    const size = Math.min(BACKUP_CHUNK_BYTES, FLASH_BYTES - offset);
    bytes.set(await verifiedRead(loader, transport, offset, size), offset);
    check();
    reportProgress((offset + size) / FLASH_BYTES);
  }
  const sector = bytes.subarray(TABLE_OFFSET, TABLE_OFFSET + SECTOR_BYTES);
  verifyLayout(sector, source.partitions);
  if (await sha256(sector) !== source.sectorSha256)
    throw stop("The backup does not match the inspected factory layout.");
  const digest = md5(bytes);
  check();
  if (await loader.flashMd5sum(0, FLASH_BYTES) !== digest)
    throw stop("The complete backup did not match a second device checksum.");
  const backup = Object.freeze({ bytes, sha256: await sha256(bytes), md5: digest, sourceSectorSha256: source.sectorSha256 });
  check();
  verifiedBackups.add(backup);
  return backup;
}

async function verifyBackup(backup) {
  if (!verifiedBackups.has(backup) || !(backup.bytes instanceof Uint8Array) ||
      backup.bytes.length !== FLASH_BYTES || backup.sourceSectorSha256 !== APPROVED_FACTORY_SOURCE.sectorSha256 ||
      await sha256(backup.bytes) !== backup.sha256 || md5(backup.bytes) !== backup.md5)
    throw stop("The verified recovery backup is missing or changed.");
}

// The caller supplies a local file saver that resolves true only after closing
// the file and reading it back with a matching SHA-256. A download click alone
// cannot establish that a recovery copy was successfully saved.
export async function saveFactoryBackup(backup, save) {
  await verifyBackup(backup);
  if (typeof save !== "function" || await save(backup) !== true)
    throw stop("Save and verify the recovery backup before replacing factory firmware.");
  await verifyBackup(backup);
  savedBackups.add(backup);
  return backup;
}

export async function revalidateFactoryBeforeWrite(loader, transport, backup, source) {
  exactSource(source);
  await verifyBackup(backup);
  if (!savedBackups.has(backup)) throw stop("The recovery backup has not been saved and verified.");
  const sector = await verifiedRead(loader, transport, TABLE_OFFSET, SECTOR_BYTES);
  verifyLayout(sector, source.partitions);
  if (await sha256(sector) !== source.sectorSha256 || await loader.flashMd5sum(0, FLASH_BYTES) !== backup.md5)
    throw stop("The device changed after its recovery backup. Reconnect and start again.");
}

export async function verifyFactoryAfterWrite(loader, transport, release) {
  try {
    verifyTargetRelease(release);
    const sector = await verifiedRead(loader, transport, TABLE_OFFSET, SECTOR_BYTES);
    verifyLayout(sector, TARGET_PARTITIONS);
    if (await sha256(sector) !== TARGET_SECTOR_SHA256)
      throw new Error("The installed partition table did not verify.");
    const selector = await verifiedRead(loader, transport, 0xe000, 0x2000);
    if (!selector.every(byte => byte === 255)) throw new Error("The new app0 boot selection did not verify.");
  } catch (error) {
    throw new Error(`${error.message.replace(/ Nothing was written\.$/, "")} Keep the recovery backup.`);
  }
}

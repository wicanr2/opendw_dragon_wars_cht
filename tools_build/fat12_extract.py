#!/usr/bin/env python3
"""Human68k / MS-DOS FAT12 extractor for X68000 Dragon Wars disks.

Layout (derived empirically from disk images):
  sector size      = 1024
  boot sectors     = 1   (0x000..0x400)
  FAT count        = 2
  FAT size         = 2 sectors each  (0x400..0xC00, 0xC00..0x1400)
  root dir         = 1 sector, 32 entries (0x1400..0x1800)
  data area        = cluster 2 @ 0x1800, cluster size = 1 sector
"""
import struct
import sys
import os

SECTOR = 1024
BOOT = 1
NFAT = 2
FATSZ = 2          # sectors per FAT
ROOTDIR_SECTORS = 1
ROOT_ENTRIES = 32


def load(path):
    with open(path, "rb") as f:
        return f.read()


def fat_offset():
    return BOOT * SECTOR


def rootdir_offset():
    return (BOOT + NFAT * FATSZ) * SECTOR


def data_offset():
    return (BOOT + NFAT * FATSZ + ROOTDIR_SECTORS) * SECTOR


def read_fat12(img):
    base = fat_offset()
    fat = img[base:base + FATSZ * SECTOR]
    # decode all 12-bit entries
    entries = []
    n = (len(fat) * 8) // 12
    for i in range(n):
        b = (i * 3) // 2
        if b + 1 >= len(fat):
            break
        if i % 2 == 0:
            val = fat[b] | ((fat[b + 1] & 0x0F) << 8)
        else:
            val = (fat[b] >> 4) | (fat[b + 1] << 4)
        entries.append(val & 0xFFF)
    return entries


def cluster_chain(fat, start):
    chain = []
    c = start
    guard = 0
    while 2 <= c < 0xFF0 and guard < 4000:
        chain.append(c)
        c = fat[c]
        guard += 1
    return chain


def read_file(img, fat, start_cluster, size):
    chain = cluster_chain(fat, start_cluster)
    data = bytearray()
    for c in chain:
        off = data_offset() + (c - 2) * SECTOR
        data += img[off:off + SECTOR]
    return bytes(data[:size])


def parse_root(img):
    base = rootdir_offset()
    out = []
    for i in range(ROOT_ENTRIES):
        e = img[base + i * 32: base + i * 32 + 32]
        if len(e) < 32:
            break
        first = e[0]
        if first == 0x00:
            break
        if first == 0xE5:
            continue  # deleted
        attr = e[11]
        if attr & 0x08:  # volume label
            continue
        if attr & 0x10:  # directory (none expected)
            pass
        name = e[0:8].decode("ascii", "replace").rstrip()
        ext = e[8:11].decode("ascii", "replace").rstrip()
        start = struct.unpack("<H", e[26:28])[0]
        size = struct.unpack("<I", e[28:32])[0]
        fname = name + ("." + ext if ext else "")
        out.append((fname, start, size, attr))
    return out


def main():
    img_path = sys.argv[1]
    outdir = sys.argv[2]
    os.makedirs(outdir, exist_ok=True)
    img = load(img_path)
    fat = read_fat12(img)
    entries = parse_root(img)
    print(f"# {img_path}: root_off=0x{rootdir_offset():x} data_off=0x{data_offset():x}")
    for fname, start, size, attr in entries:
        data = read_file(img, fat, start, size)
        outp = os.path.join(outdir, fname)
        with open(outp, "wb") as f:
            f.write(data)
        print(f"  {fname:14s} clus={start:4d} size={size:7d} attr=0x{attr:02x} -> {len(data)} bytes")


if __name__ == "__main__":
    main()

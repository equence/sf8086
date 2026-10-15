#!/usr/bin/env python3
"""FAT12 image editor for the 8086tiny boot floppy.

The boot image is a 1.44 MB FAT12 volume that FreeDOS boots from, so it has to be
edited in place: the boot sector, both FAT copies, the fixed root directory and
the existing directory tree all have to stay consistent.  This tool does that
and nothing else - no reformatting, no geometry changes.

Commands:
    ls [path]                 list a directory (default: root)
    free                      free space summary
    check                     walk the tree, report lost clusters / bad chains
    add <hostfile> [dosdir]   add a file as 8.3 (dosdir defaults to root)
    del <dospath>             delete a file (frees its chain)
    reclaim                   rebuild the FAT from what the tree actually uses
    deleted [pattern]         list recoverable deleted entries (data intact)
    undelete <name> [dosdir]  restore a deleted entry into a directory
    put <hostdir> <dosdir>    add every file in a host directory

Examples:
    python3 tools/floppy/fat12.py fd.img check
    python3 tools/floppy/fat12.py fd.img undelete EDIT.EXE
    python3 tools/floppy/fat12.py fd.img add payload/TC/TCC.EXE FREEDOS/BIN
"""

import os
import struct
import sys
from datetime import datetime

CLUSTER_EOF = 0xFFF
ATTR_READONLY = 0x01
ATTR_HIDDEN = 0x02
ATTR_SYSTEM = 0x04
ATTR_VOLUME = 0x08
ATTR_DIRECTORY = 0x10
ATTR_ARCHIVE = 0x20
ATTR_LFN = 0x0F


class Fat12:
    def __init__(self, path, writable=False):
        self.path = path
        with open(path, "rb") as handle:
            self.data = bytearray(handle.read())
        self.writable = writable

        if self.data[510:512] != b"\x55\xAA":
            raise SystemExit(f"{path}: no boot signature, refusing to touch it")

        (self.bytes_per_sector, self.sectors_per_cluster, self.reserved_sectors,
         self.num_fats, self.root_entries, self.total_sectors, self.media,
         self.sectors_per_fat) = struct.unpack_from("<HBHBHHBH", self.data, 11)

        if self.bytes_per_sector != 512 or self.sectors_per_cluster != 1:
            raise SystemExit("unsupported geometry: only 512-byte clusters are handled")

        self.fat_offset = self.reserved_sectors * self.bytes_per_sector
        self.root_offset = self.fat_offset + self.num_fats * self.sectors_per_fat * self.bytes_per_sector
        self.root_size = self.root_entries * 32
        self.data_offset = self.root_offset + self.root_size
        self.cluster_count = (self.total_sectors - (self.data_offset // self.bytes_per_sector))
        self.image_size = len(self.data)
        self.dirty = False

    # ---------------------------------------------------------------- FAT bits
    def fat_get(self, cluster, fat_index=0):
        base = self.fat_offset + fat_index * self.sectors_per_fat * self.bytes_per_sector
        offset = base + (cluster * 3) // 2
        value = struct.unpack_from("<H", self.data, offset)[0]
        return (value >> 4) if (cluster & 1) else (value & 0x0FFF)

    def fat_set(self, cluster, value):
        value &= 0x0FFF
        for fat_index in range(self.num_fats):
            base = self.fat_offset + fat_index * self.sectors_per_fat * self.bytes_per_sector
            offset = base + (cluster * 3) // 2
            current = struct.unpack_from("<H", self.data, offset)[0]
            if cluster & 1:
                current = (current & 0x000F) | ((value << 4) & 0xFFF0)
            else:
                current = (current & 0xF000) | value
            struct.pack_into("<H", self.data, offset, current)
        self.dirty = True

    def cluster_offset(self, cluster):
        return self.data_offset + (cluster - 2) * self.bytes_per_sector

    def cluster_bytes(self, cluster):
        offset = self.cluster_offset(cluster)
        return bytes(self.data[offset:offset + self.bytes_per_sector])

    def chain(self, first):
        seen = set()
        cluster = first
        while 2 <= cluster < CLUSTER_EOF:
            if cluster in seen:
                raise SystemExit(f"cluster chain loop at {cluster}")
            seen.add(cluster)
            yield cluster
            cluster = self.fat_get(cluster)

    def is_free(self, cluster):
        return self.fat_get(cluster) == 0

    # ------------------------------------------------------------ directories
    def dir_region(self, cluster):
        if cluster == 0:
            return self.root_offset, self.root_entries
        return self.cluster_offset(cluster), self.bytes_per_sector // 32

    def dir_entries(self, cluster):
        yield from self.dir_slots(cluster)

    @staticmethod
    def entry_name(raw):
        if raw[0] in (0x00, 0xE5, ord(".")):
            return None
        stem = raw[:8].decode("ascii", "replace").rstrip()
        ext = raw[8:11].decode("ascii", "replace").rstrip()
        return stem + ("." + ext if ext else "")

    def find_dir(self, path):
        """Return (cluster, entries_index_path) for a DOS directory path."""
        cluster = 0
        parts = [p for p in path.replace("\\", "/").split("/") if p and p != "."]
        for part in parts:
            found = None
            for offset, raw in self.dir_entries(cluster):
                if raw[0] == 0x00:
                    break
                name = self.entry_name(raw)
                if name and (raw[11] & ATTR_DIRECTORY) and name.upper() == part.upper():
                    found = struct.unpack_from("<H", raw, 26)[0]
                    break
            if found is None:
                raise SystemExit(f"no such directory: {part}")
            cluster = found
        return cluster

    def list_dir(self, cluster, indent=0):
        rows = []
        for _offset, raw in self.dir_slots(cluster):
            if raw[0] in (0x00, 0xE5):
                continue
            if (raw[11] & ATTR_LFN) == ATTR_LFN:
                continue
            name = self.entry_name(raw)
            if name is None:
                continue
            size = struct.unpack_from("<I", raw, 28)[0]
            first = struct.unpack_from("<H", raw, 26)[0]
            rows.append((name, raw[11], size, first))
        return rows

    def dir_slots(self, cluster):
        """Yield every 32-byte slot of a directory, following its whole chain."""
        groups = [0] if cluster == 0 else list(self.chain(cluster))
        for group in groups:
            base, count = self.dir_region(group)
            for index in range(count):
                offset = base + index * 32
                yield offset, bytes(self.data[offset:offset + 32])

    def entry_exists(self, dir_cluster, name):
        for _offset, raw in self.dir_slots(dir_cluster):
            found = self.entry_name(raw)
            if found and found.upper() == name.upper():
                return True
        return False

    def walk(self, cluster=0, prefix=""):
        for name, attr, size, first in self.list_dir(cluster):
            path = f"{prefix}/{name}" if prefix else name
            if attr & ATTR_DIRECTORY:
                yield path, attr, size, first, True
                yield from self.walk(first, path)
            else:
                yield path, attr, size, first, False

    def free_slot(self, dir_cluster, extend=True):
        """
        Return the offset of a free 32-byte directory slot, growing the directory
        by one cluster when it is full (directories are ordinary cluster chains).
        """
        last = None
        groups = [0] if dir_cluster == 0 else list(self.chain(dir_cluster))
        for group in groups:
            last = group if group else last
            base, count = self.dir_region(group)
            for index in range(count):
                offset = base + index * 32
                if self.data[offset] in (0x00, 0xE5):
                    return offset

        if not extend or last is None:
            raise SystemExit("directory is full (no free entry)")

        new_cluster = self.allocate_cluster()
        start = self.cluster_offset(new_cluster)
        self.data[start:start + self.bytes_per_sector] = b"\x00" * self.bytes_per_sector
        self.fat_set(last, new_cluster)
        self.fat_set(new_cluster, CLUSTER_EOF)
        return start

    def allocate_cluster(self):
        for cluster in range(2, self.cluster_count + 2):
            if self.is_free(cluster):
                self.fat_set(cluster, CLUSTER_EOF)
                return cluster
        raise SystemExit("disk full")

    def add_entry(self, dir_cluster, name, size, first_cluster):
        stem, _, ext = name.partition(".")
        if len(stem) > 8 or len(ext) > 3:
            raise SystemExit(f"{name}: not an 8.3 name")
        raw = bytearray(32)
        raw[0:8] = stem.upper().ljust(8).encode("ascii")
        raw[8:11] = ext.upper().ljust(3).encode("ascii")
        raw[11] = ATTR_ARCHIVE
        now = datetime.now()
        struct.pack_into("<H", raw, 22, (now.hour << 11) | (now.minute << 5) | (now.second // 2))
        struct.pack_into("<H", raw, 24, ((now.year - 1980) << 9) | (now.month << 5) | now.day)
        struct.pack_into("<H", raw, 26, first_cluster)
        struct.pack_into("<I", raw, 28, size)

        offset = self.free_slot(dir_cluster)
        self.data[offset:offset + 32] = raw
        self.dirty = True

    def make_dir(self, parent_cluster, name):
        cluster = self.allocate_cluster()
        start = self.cluster_offset(cluster)
        self.data[start:start + self.bytes_per_sector] = b"\x00" * self.bytes_per_sector
        self.fat_set(cluster, CLUSTER_EOF)

        # "." and ".." as DOS expects them
        dot = bytearray(32)
        dot[0:8] = b".       "
        dot[8:11] = b"   "
        dot[11] = ATTR_DIRECTORY
        struct.pack_into("<H", dot, 26, cluster)
        self.data[start:start + 32] = dot

        dotdot = bytearray(32)
        dotdot[0:8] = b"..      "
        dotdot[8:11] = b"   "
        dotdot[11] = ATTR_DIRECTORY
        struct.pack_into("<H", dotdot, 26, parent_cluster)
        self.data[start + 32:start + 64] = dotdot

        # The entry for the new directory itself lives in its parent, not here.
        stem, _, ext = name.partition(".")
        raw = bytearray(32)
        raw[0:8] = stem.upper().ljust(8).encode("ascii")[:8]
        raw[8:11] = ext.upper().ljust(3).encode("ascii")[:3]
        raw[11] = ATTR_DIRECTORY
        now = datetime.now()
        struct.pack_into("<H", raw, 22, (now.hour << 11) | (now.minute << 5) | (now.second // 2))
        struct.pack_into("<H", raw, 24, ((now.year - 1980) << 9) | (now.month << 5) | now.day)
        struct.pack_into("<H", raw, 26, cluster)

        offset = self.free_slot(parent_cluster)
        self.data[offset:offset + 32] = raw
        self.dirty = True
        return cluster

    def remove_entry(self, dir_cluster, name):
        for offset, raw in self.dir_slots(dir_cluster):
            if self.entry_name(raw) and self.entry_name(raw).upper() == name.upper():
                first = struct.unpack_from("<H", raw, 26)[0]
                size = struct.unpack_from("<I", raw, 28)[0]
                # Materialise the chain before touching the FAT: clearing an
                # entry while walking would cut the walk short.
                for cluster in list(self.chain(first)):
                    self.fat_set(cluster, 0)
                self.data[offset] = 0xE5
                self.dirty = True
                return size
        raise SystemExit(f"{name}: not found")

    # -------------------------------------------------------------- file data
    def read_file(self, entry_cluster, size):
        out = bytearray()
        for cluster in self.chain(entry_cluster):
            out += self.cluster_bytes(cluster)
        return bytes(out[:size])

    def allocate(self, blob, dir_cluster, name):
        needed = (len(blob) + self.bytes_per_sector - 1) // self.bytes_per_sector

        # Free clusters are claimed from the whole data area; a full check first,
        # so a failed add cannot leave a half-written chain behind.
        free = [c for c in range(2, self.cluster_count + 2) if self.is_free(c)]
        if len(free) < needed:
            raise SystemExit(f"{name}: needs {needed} clusters, only {len(free)} free")

        first = free[0] if needed else 0
        for index in range(needed):
            cluster = free[index]
            next_cluster = free[index + 1] if index + 1 < needed else CLUSTER_EOF
            self.fat_set(cluster, next_cluster)
            start = self.cluster_offset(cluster)
            chunk = blob[index * self.bytes_per_sector:(index + 1) * self.bytes_per_sector]
            self.data[start:start + len(chunk)] = chunk
            if len(chunk) < self.bytes_per_sector:
                self.data[start + len(chunk):start + self.bytes_per_sector] = b"\x00" * (
                    self.bytes_per_sector - len(chunk))
        self.add_entry(dir_cluster, name, len(blob), first)
        return needed

    def save(self):
        if not self.dirty:
            return
        if not self.writable:
            raise SystemExit("image opened read-only")
        with open(self.path, "wb") as handle:
            handle.write(self.data)

    # ------------------------------------------------------------- utilities
    def used_clusters(self):
        used = set()
        for _path, attr, size, first, is_dir in self.walk():
            if first >= 2:
                for cluster in self.chain(first):
                    used.add(cluster)
        return used

    def lost_clusters(self):
        used = self.used_clusters()
        return [c for c in range(2, self.cluster_count + 2)
                if not self.is_free(c) and c not in used]

    def reclaim(self):
        lost = self.lost_clusters()
        for cluster in lost:
            self.fat_set(cluster, 0)
        return len(lost)

    def deleted_entries(self):
        """Find deleted directory entries whose cluster chain is still intact."""
        found = []
        for cluster in range(2, self.cluster_count + 2):
            offset = self.cluster_offset(cluster)
            if self.fat_get(cluster) == 0 and bytes(self.data[offset:offset + 32]) == b"\x00" * 32:
                continue
            for index in range(self.bytes_per_sector // 32):
                entry = offset + index * 32
                raw = bytes(self.data[entry:entry + 32])
                if raw[0] != 0xE5:
                    continue
                attr = raw[11]
                if attr in (0x00, 0xFF) or (attr & ATTR_LFN) == ATTR_LFN:
                    continue
                size = struct.unpack_from("<I", raw, 28)[0]
                first = struct.unpack_from("<H", raw, 26)[0]
                stem = raw[1:8].decode("ascii", "replace").rstrip()
                ext = raw[8:11].decode("ascii", "replace").rstrip()
                if not stem or any(ch < " " or ch == "\x00" for ch in stem):
                    continue
                name = stem + ("." + ext if ext else "")
                if attr & ATTR_DIRECTORY:
                    continue
                if first < 2:
                    continue
                try:
                    clusters = list(self.chain(first))
                except SystemExit:
                    continue
                capacity = len(clusters) * self.bytes_per_sector
                if not (size <= capacity < size + 2 * self.bytes_per_sector):
                    continue
                found.append({
                    "name": name, "size": size, "first": first,
                    "clusters": len(clusters), "slot": entry,
                })
        # de-duplicate by (name, size, first)
        unique = {}
        for item in found:
            key = (item["name"], item["size"], item["first"])
            unique.setdefault(key, item)
        return sorted(unique.values(), key=lambda item: -item["size"])

    @staticmethod
    def _plausible_entry(raw):
        if raw[0] in (0x00, 0xE5, ord(".")):
            return False
        if (raw[11] & ATTR_LFN) == ATTR_LFN or raw[11] in (0x00, 0xFF):
            return False
        if not all(0x20 <= ch < 0x7F for ch in raw[:11]):
            return False
        return raw[:1] != b" "

    def orphaned_entries(self):
        """
        Find directory entries that live in clusters the tree can no longer reach.

        The boot image was built by dropping whole directories out of the tree
        without freeing anything, so their entries - and the data they point at -
        are still on the disk.  Those files can simply be re-linked.
        """
        used = self.used_clusters()
        found = {}
        for cluster in range(2, self.cluster_count + 2):
            if cluster in used:
                continue
            offset = self.cluster_offset(cluster)
            for index in range(self.bytes_per_sector // 32):
                slot = offset + index * 32
                raw = bytes(self.data[slot:slot + 32])
                if not self._plausible_entry(raw) or (raw[11] & ATTR_DIRECTORY):
                    continue
                size = struct.unpack_from("<I", raw, 28)[0]
                first = struct.unpack_from("<H", raw, 26)[0]
                if first < 2 or size == 0:
                    continue
                name = self.entry_name(raw)
                try:
                    clusters = list(self.chain(first))
                except SystemExit:
                    continue
                capacity = len(clusters) * self.bytes_per_sector
                if not (size <= capacity < size + 2 * self.bytes_per_sector):
                    continue
                found.setdefault((name, size, first), {
                    "name": name, "size": size, "first": first,
                    "clusters": len(clusters), "slot": slot,
                })
        return sorted(found.values(), key=lambda item: -item["size"])

    def relink(self, name, dir_cluster, source):
        """
        Re-create a directory entry for a file whose name is still recorded.

        Unlike restore() the first character is intact, so the name travels from
        the source entry verbatim.
        """
        raw = bytearray(self.data[source["slot"]:source["slot"] + 32])
        if self.entry_name(bytes(raw)) is None:
            raise SystemExit(f"{name}: source entry vanished")
        now = datetime.now()
        struct.pack_into("<H", raw, 22, (now.hour << 11) | (now.minute << 5) | (now.second // 2))
        struct.pack_into("<H", raw, 24, ((now.year - 1980) << 9) | (now.month << 5) | now.day)

        offset = self.free_slot(dir_cluster)
        self.data[offset:offset + 32] = raw
        self.dirty = True

    def restore(self, item, dir_cluster, first_char="?"):
        """Re-create a directory entry for a deleted file, keeping its data."""
        name = item["name"]
        stem, _, ext = name.partition(".")
        raw = bytearray(32)
        raw[0:8] = (first_char + stem[1:]).upper().ljust(8).encode("ascii")[:8]
        raw[8:11] = ext.upper().ljust(3).encode("ascii")[:3]
        raw[11] = ATTR_ARCHIVE
        now = datetime.now()
        struct.pack_into("<H", raw, 22, (now.hour << 11) | (now.minute << 5) | (now.second // 2))
        struct.pack_into("<H", raw, 24, ((now.year - 1980) << 9) | (now.month << 5) | now.day)
        struct.pack_into("<H", raw, 26, item["first"])
        struct.pack_into("<I", raw, 28, item["size"])

        base, count = self.dir_region(dir_cluster)
        for index in range(count):
            offset = base + index * 32
            if self.data[offset] in (0x00, 0xE5):
                self.data[offset:offset + 32] = raw
                self.dirty = True
                return name
        raise SystemExit("directory is full")


def human(size):
    return f"{size:,} B ({size / 1024:.0f} KB)"


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 1

    image_path, command = argv[1], argv[2]
    args = argv[3:]
    writable = command in ("add", "del", "reclaim", "undelete", "put", "mkdir", "salvage")
    fs = Fat12(image_path, writable=writable)

    if command == "ls":
        cluster = fs.find_dir(args[0]) if args else 0
        total = 0
        for name, attr, size, first in sorted(fs.list_dir(cluster)):
            kind = "<DIR>" if attr & ATTR_DIRECTORY else ""
            print(f"  {name:14s} {size:8d} {kind}" if kind else f"  {name:14s} {size:8d}")
            total += size
        print(f"  ({human(total)} in files)")

    elif command == "free":
        free = [c for c in range(2, fs.cluster_count + 2) if fs.is_free(c)]
        print(f"clusters: {fs.cluster_count} total, {len(free)} free = "
              f"{human(len(free) * fs.bytes_per_sector)}")
        lost = fs.lost_clusters()
        print(f"lost (allocated but unreferenced): {len(lost)} clusters = "
              f"{human(len(lost) * fs.bytes_per_sector)}")

    elif command == "check":
        used = fs.used_clusters()
        lost = fs.lost_clusters()
        files = [row for row in fs.walk() if not row[4]]
        print(f"files: {len(files)}, referenced clusters: {len(used)}, "
              f"lost clusters: {len(lost)}")
        if lost:
            print(f"  run 'reclaim' to free {human(len(lost) * fs.bytes_per_sector)}")
        # every FAT copy must agree
        fats_equal = all(fs.fat_get(c, 0) == fs.fat_get(c, fi)
                         for c in range(2, fs.cluster_count + 2)
                         for fi in range(1, fs.num_fats))
        print(f"FAT copies identical: {fats_equal}")

    elif command == "deleted":
        pattern = args[0].upper() if args else ""
        for item in fs.deleted_entries():
            if pattern and pattern not in item["name"].upper():
                continue
            print(f"  {item['name']:14s} {item['size']:8d} B  first={item['first']:5d} "
                  f"({item['clusters']} clusters)")

    elif command == "undelete":
        if not args:
            raise SystemExit("undelete needs a file name")
        target = args[0].upper()
        dest = args[1] if len(args) > 1 else ""
        matches = [item for item in fs.deleted_entries() if item["name"].upper() == target]
        if not matches:
            raise SystemExit(f"{target}: no intact deleted entry found")
        item = matches[0]
        dir_cluster = fs.find_dir(dest) if dest else 0
        name = fs.restore(item, dir_cluster, first_char=target[0])
        fs.save()
        print(f"restored {name} ({human(item['size'])}) from clusters at {item['first']}")

    elif command == "orphans":
        pattern = args[0].upper() if args else ""
        items = fs.orphaned_entries()
        for item in items:
            if pattern and pattern not in item["name"].upper():
                continue
            print(f"  {item['name']:14s} {item['size']:8d} B  first={item['first']:5d} "
                  f"({item['clusters']} clusters)")
        print(f"  ({len(items)} files, {human(sum(i['size'] for i in items))})")

    elif command == "salvage":
        if len(args) < 2:
            raise SystemExit("salvage <pattern-or-name,...> <dosdir>")
        wanted = [w.strip().upper() for w in args[0].split(",") if w.strip()]
        dest = args[1] if len(args) > 1 else ""
        dir_cluster = fs.find_dir(dest) if dest else 0
        existing = {row[0].upper() for row in fs.list_dir(dir_cluster)}
        items = fs.orphaned_entries()
        restored = 0
        for want in wanted:
            matched = [i for i in items if i["name"].upper() == want]
            if not matched:
                print(f"  {want:14s} not found")
                continue
            if want in existing:
                print(f"  {want:14s} already present, skipped")
                continue
            item = matched[0]
            fs.relink(item["name"], dir_cluster, item)
            restored += 1
            print(f"  {item['name']:14s} {human(item['size'])} re-linked from {item['first']}")
        fs.save()
        print(f"restored {restored} files into {'/' + dest if dest else 'root'}")

    elif command == "add":
        if len(args) < 1:
            raise SystemExit("add needs a host file")
        host = args[0]
        name = os.path.basename(host).upper()
        if len(args) > 1:
            dir_cluster = fs.find_dir(args[1])
        else:
            dir_cluster = 0
        with open(host, "rb") as handle:
            blob = handle.read()
        used = fs.allocate(blob, dir_cluster, name)
        fs.save()
        print(f"added {name} ({human(len(blob))}, {used} clusters)")

    elif command == "put":
        host_dir, dest = args[0], args[1]
        dir_cluster = fs.find_dir(dest) if dest else 0
        for entry in sorted(os.listdir(host_dir)):
            if fs.entry_exists(dir_cluster, entry.upper()):
                print(f"  {entry.upper():14s} already present, skipped")
                continue
            full = os.path.join(host_dir, entry)
            if not os.path.isfile(full):
                continue
            with open(full, "rb") as handle:
                blob = handle.read()
            used = fs.allocate(blob, dir_cluster, entry.upper())
            print(f"  {entry.upper():14s} {human(len(blob))} ({used} clusters)")
        fs.save()

    elif command == "mkdir":
        dos_path = args[0].replace("\\", "/")
        parts = [p for p in dos_path.split("/") if p]
        parent = fs.find_dir("/".join(parts[:-1])) if len(parts) > 1 else 0
        cluster = fs.make_dir(parent, parts[-1])
        fs.save()
        print(f"created {dos_path} (cluster {cluster})")

    elif command == "del":
        if not args:
            raise SystemExit("del needs a DOS path")
        dos_path = args[0].replace("\\", "/")
        parts = dos_path.split("/")
        directory = "/".join(parts[:-1])
        dir_cluster = fs.find_dir(directory) if directory else 0
        size = fs.remove_entry(dir_cluster, parts[-1])
        fs.save()
        print(f"deleted {parts[-1]} ({human(size)})")

    elif command == "reclaim":
        count = fs.reclaim()
        fs.save()
        print(f"reclaimed {count} lost clusters = {human(count * fs.bytes_per_sector)}")

    else:
        print(__doc__)
        return 1

    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))

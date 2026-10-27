# Python start-up shim used by isi/scripts/build.sh --ntfs-order (on PYTHONPATH only then).
#
# MicroPython's build tools (frozen-manifest walk, compressed-message and qstr collection) emit
# content in os.listdir/os.scandir order. NTFS lists names sorted by upper-cased name; ext4 in
# hash order, which permutes the frozen-module tables and the compressed error strings. This
# makes both calls return NTFS order. It breaks stedgeai, so build.sh builds the `firmware`
# goal only (no ROMFS).
import os

_scandir = os.scandir
_listdir = os.listdir


def _key(name):
    if isinstance(name, bytes):
        name = name.decode("utf-8", "surrogateescape")
    return name.upper()


class _SortedScandir:
    def __init__(self, it):
        self._it = it
        self._entries = iter(sorted(it, key=lambda e: _key(e.name)))

    def __iter__(self):
        return self

    def __next__(self):
        return next(self._entries)

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.close()

    def close(self):
        self._it.close()


def scandir(path="."):
    return _SortedScandir(_scandir(path))


def listdir(path="."):
    return sorted(_listdir(path), key=_key)


os.scandir = scandir
os.listdir = listdir

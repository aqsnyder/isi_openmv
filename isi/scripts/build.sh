#!/bin/bash
# Build OPENMV_N6 firmware.bin for Ranger. Never a bootloader or a combined image.
#
#   isi/scripts/build.sh --out=DIR [--src=DIR] [--ntfs-order] [--mark-dirty] [--build-date=YYYY-MM-DD]
#
# --src         OpenMV tree to build (default: this checkout), submodules initialised with
#               isi/scripts/submodules.sh.
# --ntfs-order  List directories in NTFS order, as the shipped build did (see ntfs_order/).
#               Use it for every build: without it the code layout depends on the filesystem.
# --mark-dirty  Reproduce the shipped ".dirty" version suffix: flip the exec bit on README.md
#               in both trees for the duration. Nothing compiled reads README.md.
# --build-date  MICROPY_BUILD_DATE, the only date stamped into the binary. Default: the commit
#               date of HEAD, so a rebuild of the same commit is byte-identical.
set -euo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)
SRC=$(git -C "$HERE" rev-parse --show-toplevel)
OUT=
NTFS_ORDER=0
MARK_DIRTY=0
BUILD_DATE=
for arg in "$@"; do
  case "$arg" in
    --out=*) OUT=${arg#--out=} ;;
    --src=*) SRC=${arg#--src=} ;;
    --ntfs-order) NTFS_ORDER=1 ;;
    --mark-dirty) MARK_DIRTY=1 ;;
    --build-date=*) BUILD_DATE=${arg#--build-date=} ;;
    *) echo "unknown option: $arg" >&2; exit 2 ;;
  esac
done
[ -n "$OUT" ] || { echo "--out=DIR is required" >&2; exit 2; }
mkdir -p "$OUT"
OUT=$(cd "$OUT" && pwd)
SRC=$(cd "$SRC" && pwd)
cd "$SRC"

if [ -n "$BUILD_DATE" ]; then
  SOURCE_DATE_EPOCH=$(date -u -d "${BUILD_DATE}T12:00:00Z" +%s)
else
  SOURCE_DATE_EPOCH=$(git log -1 --format=%ct)
fi
export SOURCE_DATE_EPOCH

if [ "$MARK_DIRTY" = 1 ]; then
  trap 'chmod -x README.md lib/micropython/README.md' EXIT
  chmod +x README.md lib/micropython/README.md
fi
if [ "$NTFS_ORDER" = 1 ]; then
  export PATH="$HERE/ntfs_order:$PATH"
  export PYTHONPATH="$HERE/ntfs_order${PYTHONPATH:+:$PYTHONPATH}"
fi

echo "openmv:      $(git rev-parse HEAD) $(git describe --tags --dirty --always --match 'v[1-9].*' --abbrev=10)"
echo "micropython: $(git -C lib/micropython rev-parse HEAD) $(git -C lib/micropython describe --tags --dirty --always --match 'v[1-9].*' --abbrev=10)"
echo "build date:  $(date -u -d "@$SOURCE_DATE_EPOCH" +%F)  ntfs-order=$NTFS_ORDER mark-dirty=$MARK_DIRTY"

make sdk
rm -rf build/OPENMV_N6
make -j"$(nproc)" -C lib/micropython/mpy-cross >/dev/null
# OMV_ENABLE_BL=0 skips the boot/ sub-make, its signing and the openmv.bin concatenation; it
# does not change how firmware.bin is compiled. Goal `firmware` stops after linking: the
# default goal goes on to build romfs0.img, which a firmware DFU never writes.
make -j"$(nproc)" TARGET=OPENMV_N6 OMV_ENABLE_BL=0 firmware 2>&1 | tee "$OUT/build.log" | tail -8

BIN=build/OPENMV_N6/bin
# Ranger cameras boot only through kwabena's private bootloader; a source-built one waits
# 1000 ms for USB on every boot. Anything bootloader-shaped here is a build error.
for f in bootloader.bin bootloader.elf bootloader.dfu openmv.bin openmv.dfu; do
  if [ -e "$BIN/$f" ]; then
    echo "error: $BIN/$f was built; refusing to continue" >&2
    exit 1
  fi
done

cp "$BIN/firmware.bin" "$BIN/firmware.elf" build/OPENMV_N6/firmware.map "$OUT/"
python3 -I "$HERE/provenance.py" --src="$SRC" --out="$OUT" \
    --ntfs-order="$NTFS_ORDER" --mark-dirty="$MARK_DIRTY" --source-date-epoch="$SOURCE_DATE_EPOCH"

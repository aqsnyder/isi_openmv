#!/bin/bash
# Rebuild the firmware.bin Ranger ships today and check it byte for byte.
#
#   isi/scripts/parity.sh --out=DIR [--work=DIR] [--repo=URL] [--branch=NAME]
#
# Source: vendor/kw-add_vbx943-8c5fd65e (kwagyeman/openmv 8c5fd65e, the only tree with the
# VB5943 driver) + openmv/micropython cf80cce8, SDK 1.6.0. The 2026-07-28 build saw:
#  - kwagyeman's tag set, whose newest tag reachable from 8c5fd65e is v2.0, so the banner
#    says v2.0-6669 (with upstream's tags the same commit describes as v5.0.0-27);
#  - both trees dirty (CRLF checkout on Windows, built from WSL);
#  - build date 2026-07-28;
#  - NTFS directory order.
# Fallback if this repository is unavailable:
#   --repo=https://github.com/kwagyeman/openmv.git --branch=kwabena/add_vbx943
set -euo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)
VENDOR_SHA=8c5fd65e084563f0398ebe0b9d6b5ea4a75ff310
V2_0_SHA=1b010e7e02cc37ba084756d20ea69e78c7ed75e7
SHIPPED_SHA256=f04908407464c6a1a637eaa0bb90faa336a946c7db7fd25a2f3d3b5f8de0c780
REPO=https://github.com/aqsnyder/isi_openmv.git
BRANCH=vendor/kw-add_vbx943-8c5fd65e
WORK=${TMPDIR:-/tmp}/isi-parity
OUT=
for arg in "$@"; do
  case "$arg" in
    --out=*) OUT=${arg#--out=} ;;
    --work=*) WORK=${arg#--work=} ;;
    --repo=*) REPO=${arg#--repo=} ;;
    --branch=*) BRANCH=${arg#--branch=} ;;
    *) echo "unknown option: $arg" >&2; exit 2 ;;
  esac
done
[ -n "$OUT" ] || { echo "--out=DIR is required" >&2; exit 2; }

rm -rf "$WORK/openmv"
mkdir -p "$WORK"
git init -q "$WORK/openmv"
cd "$WORK/openmv"
git remote add origin "$REPO"
git fetch -q --no-tags --filter=blob:none origin "+refs/heads/$BRANCH:refs/remotes/origin/vendor"
git merge-base --is-ancestor "$VENDOR_SHA" origin/vendor \
  || { echo "error: $BRANCH does not contain $VENDOR_SHA" >&2; exit 1; }
git checkout -q --detach "$VENDOR_SHA"
git tag v2.0 "$V2_0_SHA"
DESCRIBE=$(git describe --tags --always --match 'v[1-9].*' --abbrev=10)
[ "$DESCRIBE" = "v2.0-6669-g8c5fd65e08" ] \
  || { echo "error: describe is $DESCRIBE, expected v2.0-6669-g8c5fd65e08" >&2; exit 1; }

"$HERE/submodules.sh" "$WORK/openmv"
"$HERE/build.sh" --src="$WORK/openmv" --out="$OUT" --ntfs-order --mark-dirty --build-date=2026-07-28

GOT=$(sha256sum "$OUT/firmware.bin" | cut -d' ' -f1)
if [ "$GOT" != "$SHIPPED_SHA256" ]; then
  echo "PARITY FAIL: firmware.bin sha256 $GOT, shipped $SHIPPED_SHA256" >&2
  exit 1
fi
echo "PARITY OK: firmware.bin is byte-identical to the shipped build ($SHIPPED_SHA256)"

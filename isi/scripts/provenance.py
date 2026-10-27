"""Write provenance.json and hashes.txt next to a firmware.bin built by isi/scripts/build.sh.

usage: python3 -I isi/scripts/provenance.py --src=DIR --out=DIR --ntfs-order=0|1
                                            --mark-dirty=0|1 --source-date-epoch=N
"""
import datetime
import hashlib
import json
import os
import pathlib
import platform
import re
import subprocess
import sys
import zlib


def arg(name):
    for a in sys.argv[1:]:
        if a.startswith(f"--{name}="):
            return a.split("=", 1)[1]
    raise SystemExit(f"--{name}= is required")


def run(*cmd, cwd=None):
    try:
        return subprocess.check_output(cmd, cwd=cwd, text=True, stderr=subprocess.STDOUT).strip()
    except (OSError, subprocess.CalledProcessError) as e:
        return f"unavailable: {e}"


def describe(path):
    return run("git", "describe", "--tags", "--dirty", "--always", "--match", "v[1-9].*", "--abbrev=10", cwd=path)


src = pathlib.Path(arg("src"))
out = pathlib.Path(arg("out"))
sdk_version = (src / "SDK_VERSION").read_text().strip()
sdk = pathlib.Path.home() / f"openmv-sdk-{sdk_version}"
fw = (out / "firmware.bin").read_bytes()
banner = re.search(rb"OpenMV [^;]*; MicroPython [^ \x00]*", fw)
epoch = int(arg("source-date-epoch"))

firmware = {
    "file": "firmware.bin",
    "size": len(fw),
    "crc32": f"0x{zlib.crc32(fw) & 0xffffffff:08X}",
    "sha256": hashlib.sha256(fw).hexdigest(),
    "banner": banner.group().decode() if banner else None,
}
prov = {
    "target": "OPENMV_N6",
    "make": "make TARGET=OPENMV_N6 OMV_ENABLE_BL=0 firmware",
    "bootloader_built": False,
    "openmv": {"commit": run("git", "rev-parse", "HEAD", cwd=src), "describe": describe(src)},
    "micropython": {
        "commit": run("git", "rev-parse", "HEAD", cwd=src / "lib/micropython"),
        "describe": describe(src / "lib/micropython"),
    },
    "submodules": run("git", "submodule", "status", cwd=src).splitlines(),
    "sdk": {
        "version": sdk_version,
        "gcc": run(str(sdk / "gcc/bin/arm-none-eabi-gcc"), "--version").splitlines()[0],
        "python": run(str(sdk / "python/bin/python3"), "--version"),
    },
    "host": {
        "make": run("make", "--version").splitlines()[0],
        "gcc": run("gcc", "--version").splitlines()[0],
        "os": platform.platform(),
    },
    "inputs": {
        "source_date_epoch": epoch,
        "build_date": datetime.datetime.fromtimestamp(epoch, datetime.timezone.utc).strftime("%Y-%m-%d"),
        "ntfs_order": arg("ntfs-order") == "1",
        "mark_dirty": arg("mark-dirty") == "1",
    },
    "firmware": firmware,
    "built_at": datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
}
if os.environ.get("GITHUB_RUN_ID"):
    prov["ci"] = {
        k: os.environ.get(v) for k, v in {
            "repository": "GITHUB_REPOSITORY", "ref": "GITHUB_REF", "sha": "GITHUB_SHA",
            "run_id": "GITHUB_RUN_ID", "run_attempt": "GITHUB_RUN_ATTEMPT",
        }.items()
    }

(out / "provenance.json").write_text(json.dumps(prov, indent=2) + "\n")
(out / "hashes.txt").write_text(
    f"size   {firmware['size']}\ncrc32  {firmware['crc32']}\nsha256 {firmware['sha256']}\n")
print(f"firmware.bin {firmware['size']} B  crc32 {firmware['crc32']}  sha256 {firmware['sha256']}")
print(f"banner: {firmware['banner']}")

# ISI Custom OpenMV Firmware Fork — Developer Guide

> **Target Hardware:** OpenMV Cam N6 (STM32N657xx / Cortex-M55)
> **Upstream Repo:** [openmv/openmv](https://github.com/openmv/openmv)
> **ISI Fork:** [aqsnyder/openmv](https://github.com/aqsnyder/openmv)
> **Last Updated:** 2026-04-04

---

## Table of Contents

1. [Overview](#1-overview)
2. [Repository Setup](#2-repository-setup)
3. [Git Branching Strategy](#3-git-branching-strategy)
4. [Upstream Synchronization](#4-upstream-synchronization)
5. [Custom Board Profile (ISI_N6)](#5-custom-board-profile-isi_n6)
6. [Custom C Modules](#6-custom-c-modules)
7. [Build System](#7-build-system)
8. [CI/CD Pipeline](#8-cicd-pipeline)
9. [Deployment & Field Updates](#9-deployment--field-updates)
10. [Directory Map](#10-directory-map)
11. [Troubleshooting](#11-troubleshooting)

---

## 1. Overview

This repository is a **fork** of the open-source [OpenMV](https://github.com/openmv/openmv) firmware. The OpenMV project provides the MicroPython runtime, image processing libraries, and ML inference engine that runs on OpenMV camera hardware.

ISI maintains this fork to:

- Add **proprietary C modules** (sensor drivers, encryption, custom protocol) without modifying upstream source files.
- Define a **custom board profile** (`ISI_N6`) that tunes the firmware for the ISI Ranger Camera Module hardware.
- Run **automated CI/CD** that builds only the ISI target and produces production-grade firmware artifacts.
- Enable **field firmware updates** via SD card or network OTA.

### The Golden Rule

> **Never modify upstream files.** All ISI work goes into ISI-owned directories and files. This keeps upstream merges conflict-free.

---

## 2. Repository Setup

### First-Time Clone

```powershell
# Clone the ISI fork
git clone --recursive https://github.com/aqsnyder/openmv.git
cd openmv

# Verify remotes are correct
git remote -v
# Should show:
#   origin    https://github.com/aqsnyder/openmv.git (fetch/push)
#   upstream  https://github.com/openmv/openmv.git   (fetch/push)
```

### If Remotes Need Renaming

If `origin` points to `openmv/openmv` (the upstream), fix it:

```powershell
git remote rename origin upstream
git remote rename myfork origin
# Or if starting fresh:
git remote add upstream https://github.com/openmv/openmv.git
```

### Remote Layout

| Remote     | URL                                      | Purpose                          |
|------------|------------------------------------------|----------------------------------|
| `origin`   | `https://github.com/aqsnyder/openmv.git` | **ISI fork** — push target      |
| `upstream` | `https://github.com/openmv/openmv.git`  | **OpenMV mainline** — read only  |

### Submodules

The repository has several submodules defined in `.gitmodules`:

| Submodule              | Path               | Source                                      |
|------------------------|--------------------|---------------------------------------------|
| MicroPython            | `lib/micropython`  | `openmv/micropython` (branch: `openmv`)     |
| uLab                   | `modules/ulab`     | `v923z/micropython-ulab`                    |
| TF Lite Micro          | `lib/tflm/libtflm` | `openmv/libtflm`                            |
| TinyUSB                | `lib/tinyusb`      | `hathach/tinyusb`                           |
| Alif Security Toolkit  | `tools/alif`       | `micropython/alif-security-toolkit`         |

After cloning, initialize them:

```powershell
git submodule update --init --depth=1 --no-single-branch
git -C lib/micropython/ submodule update --init --depth=1
```

---

## 3. Git Branching Strategy

| Branch Pattern             | Purpose                                                    | Who Merges          |
|----------------------------|------------------------------------------------------------|---------------------|
| `isi/main`                 | Protected integration branch. All ISI code lands here.     | PR only             |
| `feature/<description>`    | Short-lived feature work branched from `isi/main`.         | Developer           |
| `sync/upstream-YYYY-MM`    | Temporary merge branch for pulling in upstream changes.    | Maintainer          |
| `release/vX.Y.Z`          | Tagged production releases cut from `isi/main`.            | Maintainer          |

### Creating a Feature Branch

```powershell
git checkout isi/main
git pull origin isi/main
git checkout -b feature/my-new-feature
# ... make changes ...
git push origin -u feature/my-new-feature
# Create PR on GitHub: feature/my-new-feature → isi/main
```

---

## 4. Upstream Synchronization

Perform monthly, or when OpenMV publishes a new release.

### Step-by-Step

```powershell
# 1. Fetch latest upstream
git fetch upstream

# 2. Start from isi/main
git checkout isi/main
git pull origin isi/main

# 3. Create a sync branch
git checkout -b sync/upstream-2026-04

# 4. Merge upstream master into the sync branch
git merge upstream/master

# 5. Resolve any conflicts
#    If the Golden Rule was followed, conflicts should only occur in:
#    - modules/micropython.mk (the one ISI-added line)
#    Upstream-only files will auto-merge cleanly.

# 6. Build and test
#    (see Build System section)

# 7. Push and create PR
git push origin -u sync/upstream-2026-04
# Create PR on GitHub: sync/upstream-2026-04 → isi/main

# 8. After merge, clean up
git branch -d sync/upstream-2026-04
git push origin --delete sync/upstream-2026-04
```

### What Can Cause Conflicts?

| File                         | Risk    | Mitigation                                                |
|------------------------------|---------|-----------------------------------------------------------|
| `modules/micropython.mk`    | Low     | ISI adds one `-include` line at the bottom                |
| `boards/ISI_N6/*`           | None    | Upstream doesn't have this directory                       |
| `modules/isi/*`             | None    | Upstream doesn't have this directory                       |
| `.github/workflows/isi_*`   | None    | Upstream doesn't have these files                          |
| Everything else              | None    | ISI doesn't modify upstream files                          |

---

## 5. Custom Board Profile (ISI_N6)

### What Is a Board Profile?

Every OpenMV target board has a directory under `boards/` containing 7–8 config files that control:

- **Which MCU, CPU, port** to build for (`omv_boardconfig.mk`)
- **Memory map, peripherals, pin assignments** (`omv_boardconfig.h`, `omv_pins.h`)
- **Bootloader partitions and XSPI config** (`omv_bootconfig.h`)
- **Which image processing algorithms** are enabled (`imlib_config.h`)
- **Which MicroPython modules** are frozen into firmware (`manifest.py`)
- **Which ML models** are built into the ROMFS partition (`romfs.json`)
- **NumPy/SciPy** settings (`ulab_config.h`)

The build system selects the board via `TARGET=ISI_N6`, which sets:
```makefile
OMV_BOARD_CONFIG_DIR := boards/ISI_N6/
```

### ISI_N6 vs Upstream OPENMV_N6

The ISI board profile starts as a copy of `boards/OPENMV_N6/` with these key modifications:

| Setting                      | OPENMV_N6 (upstream) | ISI_N6                        | Reason                         |
|------------------------------|----------------------|-------------------------------|--------------------------------|
| `OMV_USB_PID`                | `0x1206`             | `0x1207`                      | Distinguish from stock         |
| `OMV_BOARD_CFLAGS`           | (standard)           | + `-DISI_CUSTOM_BOARD=1`      | Gate ISI-specific code paths   |
| `MICROPY_PY_PROTOCOL`        | `1`                  | `0`                           | Use ISI's own protocol         |
| `MICROPY_PY_FIR`             | `1`                  | `0`                           | No thermal sensor              |
| `MICROPY_PY_TOF`             | `1`                  | `0`                           | No ToF sensor                  |
| `MICROPY_PY_IMU`             | `1`                  | `0`                           | No IMU on Ranger               |
| `MICROPY_PY_DISPLAY`         | `1`                  | `0`                           | No display                     |
| `MICROPY_PY_TV`              | `1`                  | `0`                           | No TV output                   |
| `MICROPY_PY_AUDIO`           | `1`                  | `0`                           | No audio                       |
| `OMV_BOSON_ENABLE`           | `1`                  | `0`                           | No FLIR Boson                  |
| `OMV_GENX320_ENABLE`         | `1`                  | `0`                           | No GenX320                     |
| `OMV_LEPTON_SDK_ENABLE`      | `1`                  | `0`                           | No Lepton                      |
| `manifest.py`                | Full module set      | Trimmed + ISI scripts frozen  | Smaller image, ISI features    |

### N6 Hardware Facts

| Parameter         | Value                              |
|-------------------|------------------------------------|
| MCU               | STM32N657xx                        |
| CPU Core          | ARM Cortex-M55                     |
| FPU               | fpv5-d16                           |
| Build Port        | `stm32` (not `mimxrt`)             |
| NPU               | 600 GOPS Neural-ART (STEdgeAI)     |
| ML Backend        | `MICROPY_PY_ML_STAI = 1`          |
| DRAM              | 64 MB PSRAM @ `0x90000000`        |
| SRAM              | ~3 MB total (AXISRAM1–6 + AHBSRAM)|
| External Flash    | 32 MB XSPI NOR (MX25UM51245G)     |
| HSE Frequency     | 48 MHz                             |
| Bootloader        | Signed (header v2.3, TrustZone)    |
| USB               | TinyUSB, High-Speed (OTG_HS)       |

---

## 6. Custom C Modules

### Architecture

ISI proprietary C code lives in `modules/isi/` and follows the MicroPython "User C Modules" framework. This keeps custom code **completely separate** from upstream OpenMV source.

### File Structure

```
modules/
├── micropython.mk         # Upstream (ONE line added at bottom)
├── py_image.c              # Upstream module - DO NOT EDIT
├── py_ml.c                 # Upstream module - DO NOT EDIT
├── ...                     # (other upstream modules)
└── isi/                    # ← ISI-OWNED
    ├── micropython.mk      # ISI module build rules
    ├── isi_sensors.c        # Custom sensor integration
    ├── isi_crypto.c         # AES-256-GCM encryption module
    └── isi_protocol.c       # ISI UART command protocol
```

### How It Gets Compiled

1. The top-level `Makefile` passes `USER_C_MODULES=$(TOP_DIR)` to MicroPython.
2. MicroPython finds and executes `modules/micropython.mk`.
3. The ISI-added `-include` line at the bottom of that file includes `modules/isi/micropython.mk`.
4. The ISI makefile adds `modules/isi/*.c` to `SRC_USERMOD_C`.
5. MicroPython compiles everything and links it into the firmware.

### Writing a New C Module

1. Create `modules/isi/my_module.c`
2. Use `MP_REGISTER_MODULE()` to register it
3. Rebuild: `make -j$(nproc) TARGET=ISI_N6`
4. In MicroPython: `import my_module`

### Example Module Template

```c
// modules/isi/isi_example.c
#include "py/runtime.h"
#include "py/obj.h"

static mp_obj_t isi_hello(void) {
    return mp_obj_new_str("Hello from ISI!", 15);
}
static MP_DEFINE_CONST_FUN_OBJ_0(isi_hello_obj, isi_hello);

static const mp_rom_map_elem_t isi_example_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_isi_example) },
    { MP_ROM_QSTR(MP_QSTR_hello), MP_ROM_PTR(&isi_hello_obj) },
};
static MP_DEFINE_CONST_DICT(isi_example_globals, isi_example_globals_table);

const mp_obj_module_t isi_example_module = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&isi_example_globals,
};

MP_REGISTER_MODULE(MP_QSTR_isi_example, isi_example_module);
```

---

## 7. Build System

### Prerequisites

| Tool          | How to Install                             |
|---------------|--------------------------------------------|
| ARM Toolchain | `make sdk` (downloads everything)          |
| Build Host    | Linux x86_64 or macOS arm64 (native)       |
|               | Windows → use Docker or WSL2               |
| Docker        | Alternative: `cd docker && make TARGET=ISI_N6` |

### Build Commands

```bash
# 1. Install the OpenMV SDK (one-time, or after SDK version bump)
make sdk

# 2. Build the MicroPython cross-compiler (one-time)
make -j$(nproc) -C lib/micropython/mpy-cross

# 3. Build the ISI_N6 firmware
make -j$(nproc) TARGET=ISI_N6

# 4. (Optional) Build with debug symbols
make -j$(nproc) TARGET=ISI_N6 DEBUG=1
```

### Build Artifacts

After a successful build, artifacts are in `build/bin/`:

| File               | Purpose                                              |
|--------------------|------------------------------------------------------|
| `firmware.bin`     | Main firmware binary (flash via OpenMV IDE)           |
| `firmware.elf`     | ELF with debug symbols (for GDB/J-Link debugging)    |
| `firmware.dfu`     | DFU image (alternative flashing method)               |
| `bootloader.bin`   | Bootloader binary (rarely needs updating)             |
| `bootloader.elf`   | Bootloader with debug symbols                         |
| `openmv.bin`       | Combined bootloader + firmware image                  |
| `openmv.dfu`       | Combined DFU image                                    |
| `romfs0.img`       | ROM filesystem (ML models, haar cascades)             |

### Docker Build (Windows / No Native Toolchain)

```bash
cd docker
make TARGET=ISI_N6
# Artifacts in docker/build/ISI_N6/
```

### Clean Build

```bash
make clean
make -j$(nproc) TARGET=ISI_N6
```

---

## 8. CI/CD Pipeline

### Overview

The ISI fork uses a **separate** GitHub Actions workflow (`.github/workflows/isi_build.yml`) that does NOT modify the upstream `firmware.yml`.

### Triggers

| Event        | Branches              | Conditions                                |
|--------------|-----------------------|-------------------------------------------|
| `push`       | `isi/main`, `feature/*` | Changes to ISI-relevant paths            |
| `pull_request` | `isi/main`          | Any PR targeting ISI integration branch   |
| `tag`        | `isi-v*.*.*`          | Creates a GitHub Release with artifacts   |

### Pipeline Steps

1. **Checkout** — Clone the repository
2. **Submodules** — `source tools/ci.sh && ci_update_submodules`
3. **Cache/Install SDK** — ARM toolchain, STEdgeAI tools
4. **Build mpy-cross** — MicroPython cross-compiler
5. **Build ISI_N6** — `make -j$(nproc) TARGET=ISI_N6`
6. **Package** — Copy `.bin`, `.elf`, `.dfu`, `romfs` to artifacts
7. **Upload** — GitHub Actions artifact (90-day retention)
8. **Release** — On `isi-v*` tags, create GitHub Release with zip

### Creating a Release

```powershell
git checkout isi/main
git tag isi-v1.0.0
git push origin isi-v1.0.0
# CI will build and create a GitHub Release automatically
```

---

## 9. Deployment & Field Updates

### Method A: OpenMV IDE (Development / Factory)

1. Connect OpenMV N6 via USB
2. Open OpenMV IDE → `Tools → Run Bootloader`
3. Select `isi_n6_firmware.bin` from CI artifacts
4. Bootloader writes firmware to external XSPI NOR flash
5. Camera reboots with new firmware

**Use for:** Initial factory provisioning, development iteration.

### Method B: SD Card Update (Production Field Update)

Place the following files on the SD card root:

```
/sd/
├── firmware_update.bin       # The firmware binary
├── firmware_update.sha256    # SHA-256 checksum
└── update_manifest.json      # Version and metadata
```

**`update_manifest.json` example:**

```json
{
  "version": "1.2.0",
  "target": "ISI_N6",
  "firmware_sha256": "a1b2c3d4e5f6...",
  "min_battery_pct": 50,
  "release_date": "2026-04-04"
}
```

The `_boot.py` update logic:

1. On every boot, check if `/sd/firmware_update.bin` exists
2. Validate SHA-256 checksum against `.sha256` file
3. If valid, stage the firmware and trigger a bootloader update
4. Delete the update files after successful flash
5. If invalid, log an error and boot normally

### Method C: Network OTA (Future)

For WiFi-connected Ranger units (via CYW4343):

1. Camera polls `GET https://updates.isi.com/ranger/manifest.json`
2. Compares version numbers
3. Downloads firmware to SD card
4. Runs the same SD card update flow as Method B

> **Security requirements for OTA:** Firmware images should be signed (HMAC-SHA256 or RSA). Never erase current firmware before the new one is validated. Require minimum battery level before updating.

---

## 10. Directory Map

ISI-specific additions shown with `← ISI`:

```
openmv/                              (fork root)
├── boards/
│   ├── OPENMV_N6/                    (upstream — DO NOT EDIT)
│   ├── OPENMV_RT1060/                (upstream — DO NOT EDIT)
│   ├── OPENMV_AE3/                   (upstream — DO NOT EDIT)
│   ├── ...                           (other upstream boards)
│   └── ISI_N6/                      ← ISI: Custom board profile
│       ├── omv_boardconfig.mk        MCU, port, module enables
│       ├── omv_boardconfig.h         Memory map, PLLs, peripherals
│       ├── omv_bootconfig.h          Bootloader partitions, XSPI
│       ├── omv_pins.h                GPIO pin definitions
│       ├── imlib_config.h            Image processing enables
│       ├── manifest.py               Frozen MicroPython modules
│       ├── romfs.json                ML model ROMFS entries
│       └── ulab_config.h             NumPy/SciPy settings
│
├── modules/
│   ├── micropython.mk                (1 line added at bottom)
│   ├── py_*.c                        (upstream modules — DO NOT EDIT)
│   └── isi/                         ← ISI: Custom C modules
│       ├── micropython.mk            Build rules for ISI modules
│       ├── isi_sensors.c              Sensor integration
│       ├── isi_crypto.c               Encryption module
│       └── isi_protocol.c             UART command protocol
│
├── scripts/
│   ├── examples/                     (upstream — DO NOT EDIT)
│   ├── libraries/                    (upstream — DO NOT EDIT)
│   └── isi/                         ← ISI: MicroPython application scripts
│       ├── isi_protocol.py            Python-level protocol handler
│       ├── isi_config.py              Configuration management
│       └── isi_updater.py             SD card update logic
│
├── docs/
│   ├── firmware.md                   (upstream)
│   ├── boards.md                     (upstream)
│   └── isi/                         ← ISI: Documentation
│       └── custom_fork_guide.md       THIS FILE
│
├── .github/
│   └── workflows/
│       ├── firmware.yml               (upstream CI — DO NOT EDIT)
│       └── isi_build.yml             ← ISI: Custom CI pipeline
│
├── ports/
│   ├── stm32/                        (upstream STM32 port — DO NOT EDIT)
│   ├── mimxrt/                       (upstream i.MX RT port)
│   └── ...
│
├── lib/
│   ├── micropython/                  (submodule — openmv/micropython)
│   └── ...
│
├── tools/
│   └── ci.sh                         (upstream CI helpers — DO NOT EDIT)
│
├── Makefile                           (upstream — DO NOT EDIT)
├── .gitmodules                        (upstream)
└── README.md                          (upstream)
```

---

## 11. Troubleshooting

### Build fails: "Invalid or no TARGET specified"

You forgot `TARGET=ISI_N6`:
```bash
make -j$(nproc) TARGET=ISI_N6
```

### Build fails: "OpenMV SDK not found"

Install the SDK first:
```bash
make sdk
```

### Submodule errors

Re-initialize submodules:
```bash
git submodule update --init --depth=1 --no-single-branch
git -C lib/micropython/ submodule update --init --depth=1
```

### Merge conflicts during upstream sync

If you edited an upstream file by accident, the sync will conflict. Identify the file, move your changes to an ISI-owned location, and reset the upstream file:

```bash
git checkout upstream/master -- path/to/conflicting/file
```

### ISI C module not found after `import`

1. Verify `modules/isi/micropython.mk` is being included (check the `-include` line in `modules/micropython.mk`)
2. Run `make clean && make -j$(nproc) TARGET=ISI_N6`
3. Verify `MP_REGISTER_MODULE()` is present in your `.c` file

### USB device not recognized after flashing

Check `OMV_USB_VID` and `OMV_USB_PID` in `boards/ISI_N6/omv_boardconfig.mk`. If you changed the PID, your host OS may need updated udev rules (Linux) or driver binding (Windows).

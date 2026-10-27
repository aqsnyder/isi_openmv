# ISI build of OpenMV for the Ranger N6 camera

This fork builds the OpenMV firmware for the OPENMV_N6 module in ISI's Ranger camera. It tracks
[openmv/openmv](https://github.com/openmv/openmv) and carries a short stack of ISI patches on
top of a pinned vendor base. Everything ISI-specific lives under `isi/` and
`.github/workflows/isi_n6_firmware.yml`; nothing else in the tree changes except through
labelled patches on `isi/main`.

**It builds `firmware.bin` and nothing else.** Ranger cameras boot through Kwabena's private
bootloader, which skips the USB wait when powered from VIN. A bootloader built from `boot/`
waits 1000 ms for USB on every boot and needs ST signing. Never build, keep or flash
`bootloader.bin` or the combined `openmv.bin`, and never run `make deploy`.

## Branches

| Branch | Contents | Rules |
|---|---|---|
| `master` | Mirror of `openmv/openmv` master | Fast-forward only. Never commit to it. |
| `vendor/kw-add_vbx943-8c5fd65e` | [kwagyeman/openmv](https://github.com/kwagyeman/openmv) `kwabena/add_vbx943` @ `8c5fd65e`: upstream v5.0.0 + 26 commits, plus Kwabena's VB5943 driver ("temp" commit, not upstream). The base of every Ranger build so far. | Immutable. A new driver drop gets a new `vendor/kw-add_vbx943-<sha>`. |
| `isi/main` | Vendor base + the ISI patch stack. Ranger firmware is built from here. | Each commit subject starts with a topic label (`isi:`, `n6:`, `ps5520:` …). Moves only past the bench gate below. |
| `isi/topic/<name>` | Work on top of `isi/main` (e.g. `isi/topic/boot-marks`) | Folded into `isi/main` once bench-validated |
| `isi/rebase/<upstream-describe>` | Trial of the stack on a newer upstream | Disposable until it passes the gate |

`feature/isi-fork-infrastructure` and `fix/ps5520-aec-oscillation` predate this layout. Don't build
them. The first's workflow packages a source-built bootloader and a combined image. The second's
`ps5520.c` change went upstream as `d863ba24`.

## Tags and version stamps

The firmware banner, `sys.version` on the camera, is `OpenMV <git describe>; MicroPython <git describe>`,
so the tags a clone has decide what the firmware reports.

- Upstream's `v*` tags are mirrored here (fetch `refs/tags/v*` only; upstream's rolling
  `development` tag clashes with this fork's stale copy and isn't needed).
- Every build that leaves the bench is tagged `v<upstream base>-isi.<n>` on the commit that was
  built, e.g. `v5.0.0-isi.1`. The banner then reads `OpenMV v5.0.0-isi.1; MicroPython
  v1.28.0-49.7.gcf80cce8a0`; untagged commits read `v5.0.0-isi.1.3.g<sha>`.
- `isi/shipped/2026-07-28` marks `8c5fd65e`, the source of the firmware fielded before this fork
  existed. That build was made in a clone with kwagyeman's tag set (newest reachable: `v2.0`), so it
  reports `OpenMV v2.0-6669.g8c5fd65e08.dirty`; with upstream's tags the same commit describes as
  `v5.0.0-27`.

## Build

Ubuntu 24.04 or WSL. On Windows, build inside the WSL filesystem, not under `/mnt/c`: paths get
too long there, and NTFS is slow.

```bash
git clone https://github.com/aqsnyder/isi_openmv.git && cd isi_openmv
git checkout isi/main
isi/scripts/submodules.sh
isi/scripts/build.sh --out=out --ntfs-order        # -> out/firmware.bin, .elf, .map, provenance.json
isi/scripts/parity.sh --out=parity                 # rebuilds the 2026-07-28 firmware, checks its SHA-256
```

`build.sh` runs `make sdk` (OpenMV SDK `$(cat SDK_VERSION)`, 4.6 GB, into `~/openmv-sdk-<ver>`),
then `make TARGET=OPENMV_N6 OMV_ENABLE_BL=0 firmware`. It fails if anything bootloader-shaped
appears and writes `provenance.json`: both commits and describes, submodules, toolchain, inputs,
size, CRC32, SHA-256 and banner. The build date defaults to the commit date, so rebuilding a commit
is byte-identical.

Always pass `--ntfs-order`. `common/micropy.mk` links `libmicropython.a` in raw `find` order, and
MicroPython's frozen-module and qstr tools emit in `os.listdir` order. ext4 returns hash order, which
differs between machines, so builds of the same source get different layouts. The shims in
`isi/scripts/ntfs_order/` impose the NTFS order the shipped build had. That gives byte-for-byte
reproducibility and parity with the shipped binary.

## CI

`.github/workflows/isi_n6_firmware.yml` runs on pushes and PRs to `isi/**`, on `v*-isi.*` tags
and on demand:

1. **parity**: rebuilds `vendor/kw-add_vbx943-8c5fd65e` exactly as the shipped build was made and
   fails unless `firmware.bin` has SHA-256 `f0490840…0c780`. That proves the toolchain, SDK and
   scripts still reproduce what is in the field.
2. **firmware**: builds the pushed ref with `--ntfs-order`. It uploads `firmware.bin`, `.elf`,
   `.map`, `provenance.json`, `hashes.txt` and a symbol diff against the shipped build, and puts
   the hashes and the diff in the run summary.

## Keeping up with upstream

**Sync `master`** (any time):

```bash
git remote add upstream https://github.com/openmv/openmv.git              # once
git fetch upstream master 'refs/tags/v*:refs/tags/v*'
git push origin upstream/master:refs/heads/master    # refused unless fast-forward; never force
git push origin 'refs/tags/v*'                       # upstream's new release tags
```

GitHub's "Sync fork" button (or `gh repo sync aqsnyder/isi_openmv -b master`) also moves
`master`, but it doesn't bring tags.

**New VB5943 driver drop** from Kwabena:

```bash
git remote add kw https://github.com/kwagyeman/openmv.git && git config remote.kw.tagOpt --no-tags   # once
git fetch kw kwabena/add_vbx943
git push origin <sha>:refs/heads/vendor/kw-add_vbx943-<short-sha>
```

**Move `isi/main` to a newer upstream**:

1. `git switch -c isi/rebase/<describe> upstream/master` (or a release tag).
2. Bring in the VB5943 driver: cherry-pick Kwabena's driver commit(s) and resolve.
3. Replay the ISI stack:
   `git rebase --onto isi/rebase/<describe> vendor/kw-add_vbx943-<sha> <branch copied from isi/main>`.
4. **PS5520 guard:** from v5.0.1, upstream's PS5520 `set_framesize` re-runs `sw_reset` and the full
   register table, which adds 151–155 ms to every sensor init. Check `drivers/sensors/ps5520.c`
   before accepting a rebase. A fix there touches a clause-3 file (see Licence).
5. CI green: the parity job keeps checking the shipped base, and the firmware job shows what moved.
6. **Bench gate:** flash one N6 over USB DFU (FIRMWARE partition only, alt 1 of `37c5:9206`).
   Run the boot-marks and PIR-gates protocol and compare with the current `isi/main` build. Only
   then tag the old tip `isi/archive/<date>` and
   `git push --force-with-lease origin isi/rebase/<describe>:isi/main`.

## MicroPython

`lib/micropython` is pinned to [openmv/micropython](https://github.com/openmv/micropython)
`cf80cce8` (branch `openmv-1.28.0`, not the `openmv` branch `.gitmodules` names). ISI has no
change there yet. The first one will be the Standby resume stub in
`ports/stm32/boards/OPENMV_N6/board.c`. It goes into a fork, `aqsnyder/isi_micropython`, on the
same model (mirror branch `openmv-1.28.0`, ISI stack on `isi/main`), and `isi/main` here then points
`.gitmodules` at it.

Why a fork rather than patches applied at build time:
- The submodule pin names the exact source that was built.
- Both `git describe`s stay clean, so the banner identifies the build. Build-time patches leave the
  tree `.dirty`, and different patch sets become indistinguishable.
- Rebases happen in git, where conflicts are visible, instead of in patch fuzz.
- CI needs no patch step.

MicroPython and the OpenMV board files are MIT, so a public fork raises no licence question. Push
its `v*` tags explicitly: GitHub forks don't carry them, and `MICROPY_GIT_TAG` needs them.

## Licence

These files carry clause 3 of OpenMV's licence: `drivers/sensors/ps5520.c`,
`drivers/sensors/vbx943.c`, `ports/stm32/omv_csi.c`, `ports/stm32/stm_isp.c`,
`ports/stm32/stm_xspi.c`, `modules/py_csi_ng.c` and `boot/`. ISI changes to them are not pushed
to this repository. ISI patches here touch MIT, BSD and ST-licensed files and new ISI files only.

## Repository settings

Upstream's own workflows (Firmware Build, Code Size Report, Check Code Formatting, Check Commit
Messages, Check Examples Index, Python Linter, Trigger OpenMV Studio Resources Update) serve
openmv's release flow. They build bootloaders, and they would fire here on every `master` sync and
on `v*-isi.*` tags, which match the firmware build's `v*.*.*` trigger. Keep them disabled in this
fork (Actions tab → workflow → Disable, or `gh workflow disable -R aqsnyder/isi_openmv <name>`).

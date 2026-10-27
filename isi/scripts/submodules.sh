#!/bin/bash
# Initialise the submodules of an OpenMV tree the way the shipped build saw them.
#
#   isi/scripts/submodules.sh [SRC]
#
# lib/micropython gets full commit history and its v* tags: `git describe` there becomes
# MICROPY_GIT_TAG in the firmware banner. The rest are shallow, as tools/ci.sh does.
set -euo pipefail
SRC=${1:-$(git -C "$(dirname "$0")" rev-parse --show-toplevel)}
cd "$SRC"
git submodule init
git submodule update --init --filter=blob:none lib/micropython
git -C lib/micropython fetch -q --filter=blob:none origin 'refs/tags/v*:refs/tags/v*'
git submodule update --init --depth=1 --no-single-branch
git -C lib/micropython submodule update --init --depth=1
git submodule status

# SPDX-License-Identifier: Proprietary
#
# Copyright (c) 2026 Invictus Security Wireless, LLC.
#
# ISI Custom User C Modules build rules.
# This file is included by modules/micropython.mk via -include.

ISI_MOD_DIR := $(USERMOD_DIR)/isi

# Add ISI C source files
SRC_USERMOD_C += $(wildcard $(ISI_MOD_DIR)/*.c)

# Add ISI include path
CFLAGS_USERMOD += -I$(ISI_MOD_DIR)

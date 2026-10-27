/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (C) 2026 Invictus Security Wireless.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * Boot-time marks (measurement only). See omv_boot_marks.h.
 */
#include "omv_boot_marks.h"
#if OMV_BOOT_MARKS_ENABLE
#include CMSIS_MCU_H
#include "py/mphal.h"

#define IRQ_BIT (1u << OMV_BOOT_MARK_IRQ)

omv_boot_marks_t omv_boot_marks;

const char *const omv_boot_mark_names[OMV_BOOT_MARK_COUNT] = {
    "rst", "crt", "main", "clk", "hal", "irq", "mp",
    "drv", "csi0", "csi1", "frz", "fs", "usb", "mpy",
};

uint32_t omv_boot_marks_cycles(void) {
    return DWT->CYCCNT + omv_boot_marks.cyc_base;
}

void omv_boot_marks_init(uint32_t pre_cyccnt) {
    uint32_t cycles = DWT->CYCCNT;
    uint32_t primask = __get_PRIMASK();

    omv_boot_marks.magic = OMV_BOOT_MARKS_MAGIC;
    omv_boot_marks.version = OMV_BOOT_MARKS_VERSION;
    omv_boot_marks.count = OMV_BOOT_MARK_COUNT;
    omv_boot_marks.pre_cyccnt = pre_cyccnt;

    // Reset_Handler zeroed CYCCNT on entry, so that mark is cycle 0. Neither mark
    // knows the clock (hz = 0): SystemCoreClock still holds its default until main()
    // reads the hardware, and nothing in between changes the clocks.
    omv_boot_marks.mark[OMV_BOOT_MARK_RESET].primask = primask;
    omv_boot_marks.mark[OMV_BOOT_MARK_CRT0].cycles = cycles;
    omv_boot_marks.mark[OMV_BOOT_MARK_CRT0].primask = primask;
    omv_boot_marks.recorded = (1u << OMV_BOOT_MARK_RESET) | (1u << OMV_BOOT_MARK_CRT0);
}

void omv_boot_mark(omv_boot_mark_t id) {
    uint32_t cycles = omv_boot_marks_cycles();
    uint32_t bit = 1u << id;

    // First pass after a hard reset only; soft resets leave the marks alone.
    if (omv_boot_marks.magic != OMV_BOOT_MARKS_MAGIC || (omv_boot_marks.recorded & bit)) {
        return;
    }

    omv_boot_mark_rec_t *rec = &omv_boot_marks.mark[id];
    rec->cycles = cycles;
    rec->hz = SystemCoreClock;
    rec->tick_us = mp_hal_ticks_us();
    rec->primask = __get_PRIMASK();
    omv_boot_marks.recorded |= bit;
}

void omv_boot_marks_rebase(void) {
    omv_boot_marks.cyc_base += DWT->CYCCNT;
}

void omv_boot_marks_times(int32_t us_cyc[OMV_BOOT_MARK_COUNT], int32_t us[OMV_BOOT_MARK_COUNT]) {
    const omv_boot_marks_t *m = &omv_boot_marks;
    uint32_t entry_hz = 0;
    uint64_t t_ns = 0;
    int prev = -1;

    for (int i = 0; i < OMV_BOOT_MARK_COUNT && !entry_hz; i++) {
        if (m->recorded & (1u << i)) {
            entry_hz = m->mark[i].hz;
        }
    }

    for (int i = 0; i < OMV_BOOT_MARK_COUNT; i++) {
        us_cyc[i] = us[i] = -1;
        if (!(m->recorded & (1u << i))) {
            continue;
        }
        if (prev >= 0) {
            // The interval runs at the clock in effect when it started.
            uint32_t hz = m->mark[prev].hz ? m->mark[prev].hz : entry_hz;
            if (hz) {
                t_ns += (uint64_t) (m->mark[i].cycles - m->mark[prev].cycles) * 1000000000ull / hz;
            }
        }
        us_cyc[i] = t_ns / 1000;
        if (i <= OMV_BOOT_MARK_IRQ || !(m->recorded & IRQ_BIT)) {
            us[i] = us_cyc[i];
        } else {
            us[i] = us_cyc[OMV_BOOT_MARK_IRQ] + (int32_t) (m->mark[i].tick_us - m->mark[OMV_BOOT_MARK_IRQ].tick_us);
        }
        prev = i;
    }
}

int32_t omv_boot_marks_now_us(void) {
    uint32_t tick_us = mp_hal_ticks_us();
    int32_t us_cyc[OMV_BOOT_MARK_COUNT], us[OMV_BOOT_MARK_COUNT];

    if (!(omv_boot_marks.recorded & IRQ_BIT)) {
        return -1;
    }
    omv_boot_marks_times(us_cyc, us);
    return us[OMV_BOOT_MARK_IRQ] + (int32_t) (tick_us - omv_boot_marks.mark[OMV_BOOT_MARK_IRQ].tick_us);
}
#endif // OMV_BOOT_MARKS_ENABLE

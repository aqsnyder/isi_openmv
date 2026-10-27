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
 * Boot-time marks (measurement only).
 *
 * Each mark records, once per hard reset, four 32-bit words:
 *   cycles   DWT CYCCNT since Reset_Handler entry. Reset_Handler zeroes and starts
 *            the counter; imlib_init() zeroes it again, which omv_boot_marks_rebase()
 *            folds back in. CYCCNT stops while the core sleeps in WFI, so it
 *            undercounts any mp_hal_delay_ms()/SD wait that sleeps. It only counts
 *            in Secure state if the bootloader opened secure debug (BSEC_DBGCR,
 *            writable once per reset); bootmarks.line() reports cyc=0 if it never
 *            advanced.
 *   hz       SystemCoreClock when the mark was taken: the CPU clock for the interval
 *            that starts at this mark. 0 for marks taken before main() reads it.
 *   tick_us  mp_hal_ticks_us(): SysTick-based, keeps counting through WFI, but only
 *            meaningful once SysTick is programmed and IRQs are enabled (IRQ mark).
 *   primask  PRIMASK when the mark was taken (1 = IRQs masked).
 *
 * The block lives in .bss and is reachable from Python through the `bootmarks`
 * module (bootmarks.addr() for machine.mem32 readers). Layout is versioned.
 */
#ifndef __OMV_BOOT_MARKS_H__
#define __OMV_BOOT_MARKS_H__
#include <stdint.h>

// -D$(MCU) is always on the command line; STM32N6 only once the CMSIS header is in.
#if defined(STM32N657xx)
#define OMV_BOOT_MARKS_ENABLE   (1)
#else
#define OMV_BOOT_MARKS_ENABLE   (0)
#endif

#define OMV_BOOT_MARKS_MAGIC    (0x4B524D42) // "BMRK"
#define OMV_BOOT_MARKS_VERSION  (1)

// In chronological order; marks are converted to time in this order.
typedef enum {
    OMV_BOOT_MARK_RESET,    // rst:  Reset_Handler entry (CYCCNT origin)
    OMV_BOOT_MARK_CRT0,     // crt:  .data copied, .bss zeroed, about to call main()
    OMV_BOOT_MARK_MAIN,     // main: main() entry, after SystemCoreClockUpdate()
    OMV_BOOT_MARK_CLOCKS,   // clk:  SystemClock_Config returned (oscillators and
                            //       peripheral clocks; the CPU stays on the PLL1
                            //       clock the bootloader set up)
    OMV_BOOT_MARK_HAL,      // hal:  HAL_Init returned
    OMV_BOOT_MARK_IRQ,      // irq:  storage/PSRAM/LED up, IRQs enabled
    OMV_BOOT_MARK_MPINIT,   // mp:   GC + MicroPython VM initialised
    OMV_BOOT_MARK_DRIVERS,  // drv:  low-level drivers, RTC, network stacks
    OMV_BOOT_MARK_CSI0,     // csi0: before omv_csi_init() (sensor probe)
    OMV_BOOT_MARK_CSI1,     // csi1: after omv_csi_init()
    OMV_BOOT_MARK_FROZEN,   // frz:  IMU init done, before frozen _boot.py
    OMV_BOOT_MARK_FS,       // fs:   after _boot.py (flash + SD mounted)
    OMV_BOOT_MARK_USB,      // usb:  USB device + protocol up, before boot.py
    OMV_BOOT_MARK_MAINPY,   // mpy:  before main.py
    OMV_BOOT_MARK_COUNT
} omv_boot_mark_t;

typedef struct {
    uint32_t cycles;
    uint32_t hz;
    uint32_t tick_us;
    uint32_t primask;
} omv_boot_mark_rec_t;

typedef struct {
    uint32_t magic;         // OMV_BOOT_MARKS_MAGIC once Reset_Handler has run
    uint32_t version;       // OMV_BOOT_MARKS_VERSION
    uint32_t count;         // OMV_BOOT_MARK_COUNT
    uint32_t recorded;      // bit n set = mark n recorded
    uint32_t pre_cyccnt;    // CYCCNT found at Reset_Handler entry, before zeroing
    uint32_t cyc_base;      // CYCCNT counts lost to later zeroing (imlib_init)
    omv_boot_mark_rec_t mark[OMV_BOOT_MARK_COUNT];
} omv_boot_marks_t;

extern omv_boot_marks_t omv_boot_marks;
extern const char *const omv_boot_mark_names[OMV_BOOT_MARK_COUNT];

#if OMV_BOOT_MARKS_ENABLE
// Called from Reset_Handler once .bss is zeroed.
void omv_boot_marks_init(uint32_t pre_cyccnt);
void omv_boot_mark(omv_boot_mark_t id);
// Call immediately before anything that zeroes DWT CYCCNT (imlib_init()).
void omv_boot_marks_rebase(void);
// CYCCNT plus the counts lost to rebases.
uint32_t omv_boot_marks_cycles(void);
// Microseconds since Reset_Handler entry for every mark, -1 if not recorded.
// us_cyc: from CYCCNT alone (each interval at the clock in effect at its start).
// us:     best estimate - CYCCNT up to the IRQ mark, SysTick after it, so time the
//         core spent asleep in WFI is counted. us - us_cyc after the IRQ mark is
//         the time CYCCNT missed.
void omv_boot_marks_times(int32_t us_cyc[OMV_BOOT_MARK_COUNT], int32_t us[OMV_BOOT_MARK_COUNT]);
// Microseconds since Reset_Handler entry, now (-1 before the IRQ mark).
int32_t omv_boot_marks_now_us(void);
#else
static inline void omv_boot_mark(omv_boot_mark_t id) {
}
static inline void omv_boot_marks_rebase(void) {
}
#endif
#endif // __OMV_BOOT_MARKS_H__

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
 * Boot-time marks Python module (measurement only).
 *
 *   bootmarks.line()    -> "BOOT_MARKS v1 f=800 pm=1 cyc=1 pre=0 rst=0.0 ... now=445.1 slp=12.3"
 *                          ms since Reset_Handler entry. f = CPU MHz at main(),
 *                          pm = PRIMASK at main(), cyc = 1 if CYCCNT counted,
 *                          slp = time CYCCNT missed (WFI) up to the last mark
 *   bootmarks.marks()   -> ((name, us), ...) recorded marks, best estimate
 *   bootmarks.raw()     -> (pre_cyccnt, cyc_base, ((name, cycles, hz, tick_us, primask), ...))
 *   bootmarks.now_us()  -> us since Reset_Handler entry
 *   bootmarks.addr()    -> address of omv_boot_marks_t, for machine.mem32 readers
 */
#include "omv_boot_marks.h"
#if OMV_BOOT_MARKS_ENABLE
#include "py/runtime.h"
#include "py/obj.h"

static bool py_bootmarks_recorded(int i) {
    return omv_boot_marks.recorded & (1u << i);
}

static void py_bootmarks_print_ms(vstr_t *vstr, const char *key, int32_t us) {
    if (us < 0) {
        vstr_printf(vstr, " %s=-", key);
    } else {
        vstr_printf(vstr, " %s=%d.%d", key, (int) (us / 1000), (int) ((us % 1000) / 100));
    }
}

static mp_obj_t py_bootmarks_line(void) {
    int32_t us_cyc[OMV_BOOT_MARK_COUNT], us[OMV_BOOT_MARK_COUNT];
    int32_t now = omv_boot_marks_now_us();
    int last = -1;
    vstr_t vstr;

    omv_boot_marks_times(us_cyc, us);
    vstr_init(&vstr, 256);
    vstr_printf(&vstr, "BOOT_MARKS v%u f=%u pm=%u cyc=%u pre=%u", (unsigned) omv_boot_marks.version,
                (unsigned) (omv_boot_marks.mark[OMV_BOOT_MARK_MAIN].hz / 1000000),
                (unsigned) omv_boot_marks.mark[OMV_BOOT_MARK_MAIN].primask,
                (unsigned) (omv_boot_marks.mark[OMV_BOOT_MARK_MAIN].cycles != 0),
                (unsigned) omv_boot_marks.pre_cyccnt);
    for (int i = 0; i < OMV_BOOT_MARK_COUNT; i++) {
        if (py_bootmarks_recorded(i)) {
            py_bootmarks_print_ms(&vstr, omv_boot_mark_names[i], us[i]);
            last = i;
        }
    }
    py_bootmarks_print_ms(&vstr, "now", now);
    py_bootmarks_print_ms(&vstr, "slp", (last > OMV_BOOT_MARK_IRQ) ? us[last] - us_cyc[last] : -1);
    return mp_obj_new_str_from_vstr(&vstr);
}
static MP_DEFINE_CONST_FUN_OBJ_0(py_bootmarks_line_obj, py_bootmarks_line);

static mp_obj_t py_bootmarks_marks(void) {
    int32_t us_cyc[OMV_BOOT_MARK_COUNT], us[OMV_BOOT_MARK_COUNT];
    mp_obj_t items[OMV_BOOT_MARK_COUNT];
    size_t n = 0;

    omv_boot_marks_times(us_cyc, us);
    for (int i = 0; i < OMV_BOOT_MARK_COUNT; i++) {
        if (py_bootmarks_recorded(i)) {
            mp_obj_t pair[2] = {
                mp_obj_new_str_from_cstr(omv_boot_mark_names[i]),
                mp_obj_new_int(us[i]),
            };
            items[n++] = mp_obj_new_tuple(2, pair);
        }
    }
    return mp_obj_new_tuple(n, items);
}
static MP_DEFINE_CONST_FUN_OBJ_0(py_bootmarks_marks_obj, py_bootmarks_marks);

static mp_obj_t py_bootmarks_raw(void) {
    mp_obj_t items[OMV_BOOT_MARK_COUNT];
    size_t n = 0;

    for (int i = 0; i < OMV_BOOT_MARK_COUNT; i++) {
        if (py_bootmarks_recorded(i)) {
            const omv_boot_mark_rec_t *rec = &omv_boot_marks.mark[i];
            mp_obj_t fields[5] = {
                mp_obj_new_str_from_cstr(omv_boot_mark_names[i]),
                mp_obj_new_int_from_uint(rec->cycles),
                mp_obj_new_int_from_uint(rec->hz),
                mp_obj_new_int_from_uint(rec->tick_us),
                mp_obj_new_int_from_uint(rec->primask),
            };
            items[n++] = mp_obj_new_tuple(5, fields);
        }
    }
    mp_obj_t result[3] = {
        mp_obj_new_int_from_uint(omv_boot_marks.pre_cyccnt),
        mp_obj_new_int_from_uint(omv_boot_marks.cyc_base),
        mp_obj_new_tuple(n, items),
    };
    return mp_obj_new_tuple(3, result);
}
static MP_DEFINE_CONST_FUN_OBJ_0(py_bootmarks_raw_obj, py_bootmarks_raw);

static mp_obj_t py_bootmarks_now_us(void) {
    return mp_obj_new_int(omv_boot_marks_now_us());
}
static MP_DEFINE_CONST_FUN_OBJ_0(py_bootmarks_now_us_obj, py_bootmarks_now_us);

static mp_obj_t py_bootmarks_addr(void) {
    return mp_obj_new_int_from_uint((uintptr_t) &omv_boot_marks);
}
static MP_DEFINE_CONST_FUN_OBJ_0(py_bootmarks_addr_obj, py_bootmarks_addr);

static const mp_rom_map_elem_t bootmarks_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_bootmarks) },
    { MP_ROM_QSTR(MP_QSTR_line), MP_ROM_PTR(&py_bootmarks_line_obj) },
    { MP_ROM_QSTR(MP_QSTR_marks), MP_ROM_PTR(&py_bootmarks_marks_obj) },
    { MP_ROM_QSTR(MP_QSTR_raw), MP_ROM_PTR(&py_bootmarks_raw_obj) },
    { MP_ROM_QSTR(MP_QSTR_now_us), MP_ROM_PTR(&py_bootmarks_now_us_obj) },
    { MP_ROM_QSTR(MP_QSTR_addr), MP_ROM_PTR(&py_bootmarks_addr_obj) },
};

static MP_DEFINE_CONST_DICT(bootmarks_module_globals, bootmarks_module_globals_table);

const mp_obj_module_t bootmarks_user_cmodule = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *) &bootmarks_module_globals,
};

MP_REGISTER_MODULE(MP_QSTR_bootmarks, bootmarks_user_cmodule);
#endif // OMV_BOOT_MARKS_ENABLE

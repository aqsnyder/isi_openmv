// SPDX-License-Identifier: Proprietary
//
// Copyright (c) 2026 Invictus Security Wireless, LLC.
//
// ISI custom sensor integration module.
// Exposes proprietary sensor reading functions to MicroPython.
//
// Usage from MicroPython:
//   import isi_sensors
//   value = isi_sensors.read_pir_adc()

#include "py/runtime.h"
#include "py/obj.h"

// Example: Read a custom hardware sensor value.
// Replace with actual ADC/GPIO read for the ISI Ranger hardware.
static mp_obj_t isi_read_pir_adc(void) {
    // TODO: Implement actual PIR ADC read from Ranger hardware
    return mp_obj_new_int(0);
}
static MP_DEFINE_CONST_FUN_OBJ_0(isi_read_pir_adc_obj, isi_read_pir_adc);

// Return the ISI firmware version string.
static mp_obj_t isi_fw_version(void) {
    return mp_obj_new_str("ISI-N6-v0.1.0", 14);
}
static MP_DEFINE_CONST_FUN_OBJ_0(isi_fw_version_obj, isi_fw_version);

// Module globals table
static const mp_rom_map_elem_t isi_sensors_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__),       MP_ROM_QSTR(MP_QSTR_isi_sensors) },
    { MP_ROM_QSTR(MP_QSTR_read_pir_adc),   MP_ROM_PTR(&isi_read_pir_adc_obj) },
    { MP_ROM_QSTR(MP_QSTR_fw_version),     MP_ROM_PTR(&isi_fw_version_obj) },
};
static MP_DEFINE_CONST_DICT(isi_sensors_globals, isi_sensors_globals_table);

// Module definition
const mp_obj_module_t isi_sensors_module = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&isi_sensors_globals,
};

// Register the module — always available when ISI firmware is built
MP_REGISTER_MODULE(MP_QSTR_isi_sensors, isi_sensors_module);

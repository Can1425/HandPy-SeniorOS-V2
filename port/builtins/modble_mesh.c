#include <stdlib.h>
#include <string.h>

#include "py/obj.h"
#include "py/objstr.h"
#include "py/runtime.h"
#include "mphalport.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"
#include "freertos/FreeRTOSConfig.h"
// esp-idf componants
#include "esp_system.h"
#include "esp_log.h"
/* BLE */
#include "esp_nimble_hci.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "extmod/nimble/modbluetooth_nimble.h"
#include "mesh/mesh.h"
#include "ble_mesh.h"
#include "device.h"
#include "model_on_off.h"
#include "model_level.h"
#include "model_light.h"
#include "model_sensor.h"

static const char *tag = "NimBLE_MESH";

extern volatile int mp_bluetooth_nimble_ble_state;

void ble_store_config_init(void);

extern void mp_bluetooth_deinit(void);

static mp_obj_t init() {
    mp_bluetooth_deinit(); // Clean up if necessary.

    ble_hs_cfg.reset_cb = blemesh_on_reset;
    ble_hs_cfg.sync_cb = blemesh_on_sync;
    // ble_hs_cfg.gatts_register_cb = gatts_register_cb;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

    // MP_STATE_PORT(bluetooth_nimble_root_pointers) = m_new0(mp_bluetooth_nimble_root_pointers_t, 1);
    // mp_bluetooth_gatts_db_create(&MP_STATE_PORT(bluetooth_nimble_root_pointers)->gatts_db);

    mp_bluetooth_nimble_ble_state = MP_BLUETOOTH_NIMBLE_BLE_STATE_STARTING;

    mp_bluetooth_nimble_port_preinit(); //esp_nimble_hci_and_controller_init()
    nimble_port_init();
    mp_bluetooth_nimble_port_postinit(); //do nothing.

    // By default, just register the default gap/gatt service.
    ble_svc_gap_init();
    ble_svc_gatt_init();
    bt_mesh_register_gatt();

    init_pub();

    /* XXX Need to have template for store */
    ble_store_config_init();

    mp_bluetooth_nimble_port_start();

    // Wait for sync callback
    while (mp_bluetooth_nimble_ble_state != MP_BLUETOOTH_NIMBLE_BLE_STATE_ACTIVE) {
        MICROPY_EVENT_POLL_HOOK
    }

    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_0(init_obj, init);

static mp_obj_t get_on_off(){
    return MP_OBJ_NEW_SMALL_INT(gen_onoff_user_data.onoff);
}
MP_DEFINE_CONST_FUN_OBJ_0(get_on_off_obj, get_on_off);

static mp_obj_t get_level(){
    return MP_OBJ_NEW_SMALL_INT(gen_level_user_data.level);
}
MP_DEFINE_CONST_FUN_OBJ_0(get_level_obj, get_level);

static mp_obj_t get_lightness(){
    return MP_OBJ_NEW_SMALL_INT(light_user_data.light_linear);
}
MP_DEFINE_CONST_FUN_OBJ_0(get_lightness_obj, get_lightness);

static mp_obj_t get_light_temp(){
    return MP_OBJ_NEW_SMALL_INT(light_user_data.light_temp);
}
MP_DEFINE_CONST_FUN_OBJ_0(get_light_temp_obj, get_light_temp);

static mp_obj_t get_light_hsl(){
    mp_obj_t tuple[3] = {
        tuple[0] = mp_obj_new_int(light_user_data.light_hue),
        tuple[1] = mp_obj_new_int(light_user_data.light_saturation),
        tuple[2] = mp_obj_new_int(light_user_data.light_linear),
     };
    return mp_obj_new_tuple(3, tuple);
}
MP_DEFINE_CONST_FUN_OBJ_0(get_light_hsl_obj, get_light_hsl);

static mp_obj_t set_on_off(mp_obj_t on_off){
    gen_onoff_user_data.onoff = mp_obj_get_int(on_off);
    gen_onoff_publish(&root_models[3]);
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_1(set_on_off_obj, set_on_off);

static mp_obj_t set_level(mp_obj_t level){
    gen_level_user_data.level = mp_obj_get_int(level);
    gen_level_publish(&root_models[5]);
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_1(set_level_obj, set_level);

static mp_obj_t set_lightness(mp_obj_t lightness){
    light_user_data.light_linear = mp_obj_get_int(lightness);
    light_lightness_publish(&root_models[7]);
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_1(set_lightness_obj, set_lightness);

static mp_obj_t set_light_temp(mp_obj_t light_temp){
    light_user_data.light_temp = mp_obj_get_int(light_temp);
    light_ctl_publish(&root_models[7]);
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_1(set_light_temp_obj, set_light_temp);

static mp_obj_t set_light_hsl(const mp_obj_t hsl){
    mp_obj_t *elem;
    mp_obj_get_array_fixed_n(hsl, 3, &elem);
    light_user_data.light_hue = mp_obj_get_int(elem[0]);
    light_user_data.light_saturation = mp_obj_get_int(elem[1]);
    light_user_data.light_linear = mp_obj_get_int(elem[2]);

    light_hsl_publish(&root_models[7]);
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_1(set_light_hsl_obj, set_light_hsl);

STATIC const mp_map_elem_t ble_mesh_module_locals_dict_table[] = {
    { MP_OBJ_NEW_QSTR(MP_QSTR___name__), MP_OBJ_NEW_QSTR(MP_QSTR_ble_mesh) },
    // { MP_OBJ_NEW_QSTR(MP_QSTR___init__), (mp_obj_t)&radio___init___obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_init), (mp_obj_t)&init_obj},
    // { MP_ROM_QSTR(MP_QSTR_MODELS_GEN), MP_ROM_INT(MODELS_GEN) },
    // { MP_ROM_QSTR(MP_QSTR_MODELS_LIGHT), MP_ROM_INT(MODELS_LIGHT) },
    // { MP_ROM_QSTR(MP_QSTR_MODELS_SENSOR), MP_ROM_INT(MODELS_SENSOR) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_get_on_off), (mp_obj_t)&get_on_off_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_get_level), (mp_obj_t)&get_level_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_get_lightness), (mp_obj_t)&get_lightness_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_get_light_temp), (mp_obj_t)&get_light_temp_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_get_light_hsl), (mp_obj_t)&get_light_hsl_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_set_on_off), (mp_obj_t)&set_on_off_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_set_level), (mp_obj_t)&set_level_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_set_lightness), (mp_obj_t)&set_lightness_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_set_light_temp), (mp_obj_t)&set_light_temp_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_set_light_hsl), (mp_obj_t)&set_light_hsl_obj },
    // { MP_ROM_QSTR(MP_QSTR_MODELS_SENSOR), MP_ROM_INT(MODELS_SENSOR) },
    // { MP_ROM_QSTR(MP_QSTR_MODELS_SENSOR), MP_ROM_INT(MODELS_SENSOR) },
};

STATIC MP_DEFINE_CONST_DICT(ble_mesh_module_locals_dict, ble_mesh_module_locals_dict_table);

const mp_obj_module_t mp_module_ble_mesh = {
    .base = {&mp_type_module},
    .globals = (mp_obj_dict_t *)&ble_mesh_module_locals_dict
};
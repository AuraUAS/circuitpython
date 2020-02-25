#include <stdint.h>

#include "py/obj.h"
#include "py/runtime.h"

#include "shared-bindings/uNavINS/__init__.h"
#include "shared-bindings/uNavINS/MyClass.h"

STATIC const mp_rom_map_elem_t uNavINS_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_uNavINS) },
    { MP_ROM_QSTR(MP_QSTR_MyClass), MP_ROM_PTR(&uNavINS_myclass_type) },
};

STATIC MP_DEFINE_CONST_DICT(uNavINS_module_globals, uNavINS_module_globals_table);

const mp_obj_module_t uNavINS_module = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t*)&uNavINS_module_globals,
};

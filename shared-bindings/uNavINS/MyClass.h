#ifndef MICROPY_INCLUDED_SHARED_BINDINGS_UNAVINS_MYCLASS_H
#define MICROPY_INCLUDED_SHARED_BINDINGS_UNAVINS_MYCLASS_H

#include "shared-module/uNavINS/MyClass.h"

extern const mp_obj_type_t uNavINS_myclass_type;

extern void shared_module_uNavINS_myclass_construct(uNavINS_myclass_obj_t* self);
extern void shared_module_uNavINS_myclass_deinit(uNavINS_myclass_obj_t* self);
extern bool shared_module_uNavINS_myclass_deinited(uNavINS_myclass_obj_t* self);
extern char * shared_module_uNavINS_myclass_get_question(uNavINS_myclass_obj_t* self);
extern mp_int_t shared_module_uNavINS_myclass_get_answer(uNavINS_myclass_obj_t* self);

#endif // MICROPY_INCLUDED_SHARED_BINDINGS_UNAVINS_MYCLASS_H


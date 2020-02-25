#ifndef MICROPY_INCLUDED_UNAVINS_MYCLASS_H
#define MICROPY_INCLUDED_UNAVINS_MYCLASS_H

#include "py/obj.h"

typedef struct {
  mp_obj_base_t base;
  bool deinited;
} uNavINS_myclass_obj_t;

class abc {
  float x = 1.0;
  void init() {
    x = 2.0;
  }
};

#endif // MICROPY_INCLUDED_UNAVINS_MYCLASS_H


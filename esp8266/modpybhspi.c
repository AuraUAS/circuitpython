/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 Damien P. George
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
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "ets_sys.h"
#include "etshal.h"
#include "ets_alt_task.h"

#include "py/runtime.h"
#include "py/stream.h"
#include "py/mphal.h"
#include "extmod/machine_spi.h"

#include "hspi.h"

mp_obj_t pyb_spi_make_new(const mp_obj_type_t *type, size_t n_args,
                          size_t n_kw, const mp_obj_t *args);

typedef struct _pyb_hspi_obj_t {
    mp_obj_base_t base;
    uint32_t baudrate;
    uint8_t polarity;
    uint8_t phase;
} pyb_hspi_obj_t;


STATIC void hspi_transfer(mp_obj_base_t *self_in, size_t len, const uint8_t *src, uint8_t *dest) {
    (void)self_in;

    if (dest == NULL) {
        // fast case when we only need to write data
        size_t chunk_size = 1024;
        size_t count = len / chunk_size;
        size_t i = 0;
        for (size_t j = 0; j < count; ++j) {
            for (size_t k = 0; k < chunk_size; ++k) {
                spi_tx8fast(HSPI, src[i]);
                ++i;
            }
            ets_loop_iter();
        }
        while (i < len) {
            spi_tx8fast(HSPI, src[i]);
            ++i;
        }
    } else {
        // we need to read and write data

        // Process data in chunks, let the pending tasks run in between
        size_t chunk_size = 1024; // TODO this should depend on baudrate
        size_t count = len / chunk_size;
        size_t i = 0;
        for (size_t j = 0; j < count; ++j) {
            for (size_t k = 0; k < chunk_size; ++k) {
                dest[i] = spi_transaction(HSPI, 0, 0, 0, 0, 8, src[i], 8, 0);
                ++i;
            }
            ets_loop_iter();
        }
        while (i < len) {
            dest[i] = spi_transaction(HSPI, 0, 0, 0, 0, 8, src[i], 8, 0);
            ++i;
        }
    }
}

/******************************************************************************/
// MicroPython bindings for HSPI

STATIC void pyb_hspi_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind) {
    pyb_hspi_obj_t *self = MP_OBJ_TO_PTR(self_in);
    mp_printf(print, "HSPI(id=1, baudrate=%u, polarity=%u, phase=%u)",
        self->baudrate, self->polarity, self->phase);
}

STATIC void pyb_hspi_init_helper(pyb_hspi_obj_t *self, size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    enum { ARG_id, ARG_baudrate, ARG_polarity, ARG_phase };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_id, MP_ARG_INT, {.u_int = -1} },
        { MP_QSTR_baudrate, MP_ARG_INT, {.u_int = -1} },
        { MP_QSTR_polarity, MP_ARG_INT, {.u_int = -1} },
        { MP_QSTR_phase, MP_ARG_INT, {.u_int = -1} },
    };
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args, pos_args, kw_args, MP_ARRAY_SIZE(allowed_args),
                     allowed_args, args);

    if (args[ARG_baudrate].u_int != -1) {
        self->baudrate = args[ARG_baudrate].u_int;
    }
    if (args[ARG_polarity].u_int != -1) {
        self->polarity = args[ARG_polarity].u_int;
    }
    if (args[ARG_phase].u_int != -1) {
        self->phase = args[ARG_phase].u_int;
    }
    if (self->baudrate == 80000000L) {
        // Special case for full speed.
        spi_init_gpio(HSPI, SPI_CLK_80MHZ_NODIV);
        spi_clock(HSPI, 0, 0);
    } else if (self->baudrate > 40000000L) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError,
            "impossible baudrate"));
    } else {
        uint32_t divider = 40000000L / self->baudrate;
        uint16_t prediv = MIN(divider, SPI_CLKDIV_PRE + 1);
        uint16_t cntdiv = (divider / prediv) * 2; // cntdiv has to be even
        if (cntdiv > SPI_CLKCNT_N + 1 || cntdiv == 0 || prediv == 0) {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError,
                "impossible baudrate"));
        }
        self->baudrate = 80000000L / (prediv * cntdiv);
        spi_init_gpio(HSPI, SPI_CLK_USE_DIV);
        spi_clock(HSPI, prediv, cntdiv);
    }
    // TODO: Make the byte order configurable too (discuss param names)
    spi_tx_byte_order(HSPI, SPI_BYTE_ORDER_HIGH_TO_LOW);
    spi_rx_byte_order(HSPI, SPI_BYTE_ORDER_HIGH_TO_LOW);
    CLEAR_PERI_REG_MASK(SPI_USER(HSPI), SPI_FLASH_MODE | SPI_USR_MISO |
                        SPI_USR_ADDR | SPI_USR_COMMAND | SPI_USR_DUMMY);
    // Clear Dual or Quad lines transmission mode
    CLEAR_PERI_REG_MASK(SPI_CTRL(HSPI), SPI_QIO_MODE | SPI_DIO_MODE |
                        SPI_DOUT_MODE | SPI_QOUT_MODE);
    spi_mode(HSPI, self->phase, self->polarity);
}

mp_obj_t pyb_hspi_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    mp_arg_check_num(n_args, n_kw, 0, 1, true);
    mp_int_t id = -1;
    if (n_args > 0) {
        id = mp_obj_get_int(args[0]);
    }

    if (id == -1) {
        // Multiplex to bitbanging SPI
        if (n_args > 0) {
            args++;
        }
        return pyb_spi_make_new(type, 0, n_kw, args);
    }

    if (id != 1) {
        // FlashROM is on SPI0, so far we don't support its usage
        mp_raise_ValueError("");
    }

    pyb_hspi_obj_t *self = m_new_obj(pyb_hspi_obj_t);
    self->base.type = &pyb_hspi_type;
    // set defaults
    self->baudrate = 80000000L;
    self->polarity = 0;
    self->phase = 0;
    mp_map_t kw_args;
    mp_map_init_fixed_table(&kw_args, n_kw, args + n_args);
    pyb_hspi_init_helper(self, n_args, args, &kw_args);
    return MP_OBJ_FROM_PTR(self);
}

STATIC mp_obj_t pyb_hspi_init(size_t n_args, const mp_obj_t *args, mp_map_t *kw_args) {
    pyb_hspi_init_helper(args[0], n_args - 1, args + 1, kw_args);
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_KW(pyb_hspi_init_obj, 1, pyb_hspi_init);

STATIC const mp_rom_map_elem_t pyb_hspi_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_init), MP_ROM_PTR(&pyb_hspi_init_obj) },
    { MP_ROM_QSTR(MP_QSTR_read), MP_ROM_PTR(&mp_machine_spi_read_obj) },
    { MP_ROM_QSTR(MP_QSTR_readinto), MP_ROM_PTR(&mp_machine_spi_readinto_obj) },
    { MP_ROM_QSTR(MP_QSTR_write), MP_ROM_PTR(&mp_machine_spi_write_obj) },
    { MP_ROM_QSTR(MP_QSTR_write_readinto), MP_ROM_PTR(&mp_machine_spi_write_readinto_obj) },
};

STATIC MP_DEFINE_CONST_DICT(pyb_hspi_locals_dict, pyb_hspi_locals_dict_table);

STATIC const mp_machine_spi_p_t pyb_hspi_p = {
    .transfer = hspi_transfer,
};

const mp_obj_type_t pyb_hspi_type = {
    { &mp_type_type },
    .name = MP_QSTR_HSPI,
    .print = pyb_hspi_print,
    .make_new = pyb_hspi_make_new,
    .protocol = &pyb_hspi_p,
    .locals_dict = (mp_obj_dict_t*)&pyb_hspi_locals_dict,
};

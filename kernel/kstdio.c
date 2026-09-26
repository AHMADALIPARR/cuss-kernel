/*
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * cuss-kernel
 * Copyright (C) 2026 SnapKitty Collective
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

/* kstdio.c - minimal kprintf (%c %s %d %u %x %p %%). To VGA + serial. */
#include "kstdio.h"
#include "vga.h"
#include "serial.h"
#include <stdarg.h>
#include <stdint.h>

void kputc(char c) {
    vga_putc(c);
    serial_putc(c);
}

void kputs(const char *s) {
    while (*s) kputc(*s++);
}

static void putu(uint32_t v, int base, int upper) {
    char buf[32];
    int i = 0;
    const char *dig = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    if (v == 0) { kputc('0'); return; }
    while (v) { buf[i++] = dig[v % (uint32_t)base]; v /= (uint32_t)base; }
    while (i--) kputc(buf[i]);
}

int kprintf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    for (const char *p = fmt; *p; p++) {
        if (*p != '%') { kputc(*p); continue; }
        p++;
        switch (*p) {
        case 'c': kputc((char)va_arg(ap, int)); break;
        case 's': {
            const char *s = va_arg(ap, const char *);
            kputs(s ? s : "(null)");
            break;
        }
        case 'd': {
            int32_t v = va_arg(ap, int32_t);
            if (v < 0) { kputc('-'); v = -v; }
            putu((uint32_t)v, 10, 0);
            break;
        }
        case 'u': putu(va_arg(ap, uint32_t), 10, 0); break;
        case 'x': putu(va_arg(ap, uint32_t), 16, 0); break;
        case 'X': putu(va_arg(ap, uint32_t), 16, 1); break;
        case 'p':
            kputs("0x");
            putu(va_arg(ap, uint32_t), 16, 0);
            break;
        case '%': kputc('%'); break;
        default: kputc('%'); kputc(*p); break;
        }
    }
    va_end(ap);
    return 0;
}

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

/* serial.c - COM1 polled serial, 38400 8N1. */
#include "serial.h"
#include "pic.h"

#define COM1 0x3F8

void serial_init(void) {
    io_outb(COM1 + 1, 0x00);   /* disable interrupts */
    io_outb(COM1 + 3, 0x80);   /* DLAB on */
    io_outb(COM1 + 0, 0x03);   /* 38400 baud */
    io_outb(COM1 + 1, 0x00);
    io_outb(COM1 + 3, 0x03);   /* 8N1 */
    io_outb(COM1 + 2, 0xC7);   /* FIFO on */
    io_outb(COM1 + 4, 0x0B);   /* RTS/DTR */
}

static int tx_empty(void) {
    return io_inb(COM1 + 5) & 0x20;
}

void serial_putc(char c) {
    while (!tx_empty())
        ;
    io_outb(COM1, (uint8_t)c);
}

void serial_puts(const char *s) {
    while (*s) serial_putc(*s++);
}

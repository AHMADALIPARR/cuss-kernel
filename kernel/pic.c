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

/* pic.c - 8259 PIC: remap, EOI, masking. */
#include "pic.h"

#define PIC1 0x20
#define PIC2 0xA0
#define PIC1_CMD  PIC1
#define PIC1_DATA (PIC1 + 1)
#define PIC2_CMD  PIC2
#define PIC2_DATA (PIC2 + 1)

#define ICW1_INIT 0x11
#define ICW4_8086 0x01

void pic_remap(uint8_t off1, uint8_t off2) {
    io_outb(PIC1_CMD, ICW1_INIT);
    io_wait();
    io_outb(PIC2_CMD, ICW1_INIT);
    io_wait();
    io_outb(PIC1_DATA, off1);
    io_wait();
    io_outb(PIC2_DATA, off2);
    io_wait();
    io_outb(PIC1_DATA, 0x04); /* slave at IRQ2 */
    io_wait();
    io_outb(PIC2_DATA, 0x02);
    io_wait();
    io_outb(PIC1_DATA, ICW4_8086);
    io_wait();
    io_outb(PIC2_DATA, ICW4_8086);
    io_wait();
    io_outb(PIC1_DATA, 0xFD); /* mask all but IRQ1 (keyboard) */
    io_outb(PIC2_DATA, 0xFF);
}

void pic_eoi(uint8_t irq) {
    if (irq >= 8)
        io_outb(PIC2_CMD, 0x20);
    io_outb(PIC1_CMD, 0x20);
}

void pic_mask(uint8_t irq) {
    uint16_t port = irq < 8 ? PIC1_DATA : PIC2_DATA;
    io_outb(port, (uint8_t)(io_inb(port) | (1 << (irq % 8))));
}

void pic_unmask(uint8_t irq) {
    uint16_t port = irq < 8 ? PIC1_DATA : PIC2_DATA;
    io_outb(port, (uint8_t)(io_inb(port) & ~(1 << (irq % 8))));
}

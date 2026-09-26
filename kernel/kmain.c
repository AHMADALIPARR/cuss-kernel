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

/* kmain.c - kernel entry point (C). */
#include "vga.h"
#include "serial.h"
#include "kstdio.h"
#include "idt.h"
#include "pic.h"
#include "kbd.h"
#include "shell.h"
#include <stdint.h>

#define MULTIBOOT_MAGIC 0x2BADB002

void kmain(uint32_t magic, uint32_t mb_info) {
    (void)mb_info;
    serial_init();
    vga_init();
    vga_setcolor(0x0A, 0x00); /* green on black */
    kprintf("cuss-kernel v0.1  (C11, hand-rolled)\n");
    vga_setcolor(0x07, 0x00);
    kprintf("cpu: i386 protected mode, no paging, no libc\n");

    if (magic != MULTIBOOT_MAGIC)
        kprintf("warn: not booted via multiboot (magic=0x%x)\n", magic);

    idt_init();
    pic_remap(0x20, 0x28);   /* IRQs -> vectors 32..47 */
    kbd_init();

    __asm__ volatile("sti");
    kprintf("interrupts on. starting shell.\n\n");

    shell_run(); /* never returns */
}

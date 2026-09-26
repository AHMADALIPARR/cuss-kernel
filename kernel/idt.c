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

/* idt.c - interrupt descriptor table + C-level ISR dispatcher. */
#include "idt.h"
#include "pic.h"
#include "kbd.h"
#include "kstdio.h"
#include "kstring.h"

struct idt_entry {
    uint16_t off_lo, sel;
    uint8_t zero, type;
    uint16_t off_hi;
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

static struct idt_entry idt[48];
static struct idt_ptr idtp;

extern void *isr_stubs[48]; /* defined in idt.S */

static void idt_set(int n, void *handler) {
    uint32_t h = (uint32_t)handler;
    idt[n].off_lo = (uint16_t)(h & 0xFFFF);
    idt[n].sel = 0x08;
    idt[n].zero = 0;
    idt[n].type = 0x8E; /* present, DPL0, 32-bit interrupt gate */
    idt[n].off_hi = (uint16_t)((h >> 16) & 0xFFFF);
}

void idt_init(void) {
    kmemset(idt, 0, sizeof(idt));
    for (int i = 0; i < 48; i++)
        idt_set(i, isr_stubs[i]);
    idtp.limit = (uint16_t)(sizeof(idt) - 1);
    idtp.base = (uint32_t)idt;
    __asm__ volatile("lidt %0" : : "m"(idtp));
}

static const char *exc_names[32] = {
    "divide error", "debug", "NMI", "breakpoint", "overflow",
    "bound range", "invalid opcode", "device not available",
    "double fault", "coprocessor segment", "invalid TSS",
    "segment not present", "stack fault", "general protection",
    "page fault", "reserved", "x87 FPU error", "alignment check",
    "machine check", "SIMD exception", "virtualization",
    "control protection", "reserved", "reserved", "reserved",
    "reserved", "reserved", "reserved", "reserved", "reserved",
    "reserved", "reserved",
};

void isr_handler_c(uint32_t vec, uint32_t err) {
    if (vec == 33) {          /* IRQ1: keyboard */
        kbd_irq();
        pic_eoi(1);
        return;
    }
    if (vec >= 32 && vec < 48) { /* other IRQs: acknowledge, ignore */
        pic_eoi((uint8_t)(vec - 32));
        return;
    }
    kprintf("\n*** PANIC: exception %u (%s), err=0x%x ***\nhalted.\n",
            vec, vec < 32 ? exc_names[vec] : "?", err);
    for (;;)
        __asm__ volatile("cli; hlt");
}

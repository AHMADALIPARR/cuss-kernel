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

/* vga.c - VGA text-mode driver. */
#include "vga.h"
#include "pic.h"   /* io_outb/inb */
#include "kstring.h"

static volatile uint16_t *const FB = (volatile uint16_t *)0xB8000;
static uint8_t attr = 0x07; /* light grey on black */
static int cx = 0, cy = 0;

static void move_cursor(void) {
    uint16_t pos = (uint16_t)(cy * VGA_W + cx);
    io_outb(0x3D4, 0x0F);
    io_outb(0x3D5, (uint8_t)(pos & 0xFF));
    io_outb(0x3D4, 0x0E);
    io_outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

static void scroll(void) {
    for (int y = 0; y < VGA_H - 1; y++)
        for (int x = 0; x < VGA_W; x++)
            FB[y * VGA_W + x] = FB[(y + 1) * VGA_W + x];
    for (int x = 0; x < VGA_W; x++)
        FB[(VGA_H - 1) * VGA_W + x] = (uint16_t)(attr << 8) | ' ';
}

void vga_init(void) {
    vga_clear();
}

void vga_clear(void) {
    for (int i = 0; i < VGA_W * VGA_H; i++)
        FB[i] = (uint16_t)(attr << 8) | ' ';
    cx = 0;
    cy = 0;
    move_cursor();
}

void vga_setcolor(uint8_t fg, uint8_t bg) {
    attr = (uint8_t)((bg << 4) | (fg & 0x0F));
}

void vga_putc(char c) {
    if (c == '\n') {
        cx = 0;
        cy++;
    } else if (c == '\r') {
        cx = 0;
    } else if (c == '\b') {
        if (cx > 0) {
            cx--;
            FB[cy * VGA_W + cx] = (uint16_t)(attr << 8) | ' ';
        }
    } else {
        FB[cy * VGA_W + cx] = (uint16_t)(attr << 8) | (uint8_t)c;
        cx++;
        if (cx >= VGA_W) { cx = 0; cy++; }
    }
    if (cy >= VGA_H) { scroll(); cy = VGA_H - 1; }
    move_cursor();
}

void vga_puts(const char *s) {
    while (*s) vga_putc(*s++);
}

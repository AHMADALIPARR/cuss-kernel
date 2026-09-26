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

/* vga.h - VGA text-mode driver (80x25, 0xB8000). */
#ifndef CUSS_VGA_H
#define CUSS_VGA_H

#include <stdint.h>

#define VGA_W 80
#define VGA_H 25

void vga_init(void);
void vga_clear(void);
void vga_putc(char c);
void vga_puts(const char *s);
void vga_setcolor(uint8_t fg, uint8_t bg);

#endif

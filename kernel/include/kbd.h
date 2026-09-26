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

/* kbd.h - PS/2 keyboard, IRQ1, blocking line input for the shell. */
#ifndef KBD_H
#define KBD_H

void kbd_init(void);
/* Blocking: reads a line with echo + backspace. Returns length. */
int kbd_getline(char *buf, int max);
/* Called from the IRQ1 handler. */
void kbd_irq(void);
/* Non-blocking getc: -1 when the ring is empty. */
int kbd_trygetc(void);

#endif

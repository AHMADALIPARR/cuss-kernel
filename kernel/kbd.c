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

/* kbd.c - PS/2 keyboard on IRQ1. Scancode set 1 -> ASCII ring buffer. */
#include "kbd.h"
#include "pic.h"
#include "kstdio.h"
#include "kstring.h"

#define KBD_DATA 0x60
#define KBD_BUF  256

static volatile char ring[KBD_BUF];
static volatile int head = 0, tail = 0;
static int shift = 0;

/* scancode -> ASCII, [shifted] */
static const char map[128][2] = {
    [0x02] = {'1', '!'}, [0x03] = {'2', '@'}, [0x04] = {'3', '#'},
    [0x05] = {'4', '$'}, [0x06] = {'5', '%'}, [0x07] = {'6', '^'},
    [0x08] = {'7', '&'}, [0x09] = {'8', '*'}, [0x0A] = {'9', '('},
    [0x0B] = {'0', ')'}, [0x0C] = {'-', '_'}, [0x0D] = {'=', '+'},
    [0x0E] = {'\b', '\b'},
    [0x0F] = {'\t', '\t'}, [0x10] = {'q', 'Q'}, [0x11] = {'w', 'W'},
    [0x12] = {'e', 'E'}, [0x13] = {'r', 'R'}, [0x14] = {'t', 'T'},
    [0x15] = {'y', 'Y'}, [0x16] = {'u', 'U'}, [0x17] = {'i', 'I'},
    [0x18] = {'o', 'O'}, [0x19] = {'p', 'P'}, [0x1A] = {'[', '{'},
    [0x1B] = {']', '}'}, [0x1C] = {'\n', '\n'},
    [0x1E] = {'a', 'A'}, [0x1F] = {'s', 'S'}, [0x20] = {'d', 'D'},
    [0x21] = {'f', 'F'}, [0x22] = {'g', 'G'}, [0x23] = {'h', 'H'},
    [0x24] = {'j', 'J'}, [0x25] = {'k', 'K'}, [0x26] = {'l', 'L'},
    [0x27] = {';', ':'}, [0x28] = {'\'', '"'}, [0x29] = {'`', '~'},
    [0x2B] = {'\\', '|'}, [0x2C] = {'z', 'Z'}, [0x2D] = {'x', 'X'},
    [0x2E] = {'c', 'C'}, [0x2F] = {'v', 'V'}, [0x30] = {'b', 'B'},
    [0x31] = {'n', 'N'}, [0x32] = {'m', 'M'}, [0x33] = {',', '<'},
    [0x34] = {'.', '>'}, [0x35] = {'/', '?'},
    [0x39] = {' ', ' '},
};

static void push(char c) {
    int nh = (head + 1) % KBD_BUF;
    if (nh != tail) { ring[head] = c; head = nh; }
}

static int pop(void) {
    if (head == tail) return -1;
    char c = ring[tail];
    tail = (tail + 1) % KBD_BUF;
    return (int)c;
}

void kbd_irq(void) {
    uint8_t sc = io_inb(KBD_DATA);
    if (sc == 0x2A || sc == 0x36) { shift = 1; return; }
    if (sc == 0xAA || sc == 0xB6) { shift = 0; return; }
    if (sc & 0x80) return; /* key release */
    if (sc == 0xE0) return; /* extended prefix: ignore next is fine */
    if (sc < 128 && (map[sc][0] || map[sc][1]))
        push(map[sc][shift ? 1 : 0]);
}

void kbd_init(void) {
    head = tail = 0;
    shift = 0;
    /* drain stale controller data */
    while (io_inb(0x64) & 0x01)
        (void)io_inb(KBD_DATA);
}

/* blocking getchar from the ring */
static int kbd_getc(void) {
    int c;
    for (;;) {
        c = kbd_trygetc();
        if (c >= 0) return c;
        __asm__ volatile("hlt"); /* sleep until next IRQ */
    }
}

int kbd_trygetc(void) {
    int c;
    __asm__ volatile("cli");
    c = pop();
    __asm__ volatile("sti");
    return c;
}

int kbd_getline(char *buf, int max) {
    int n = 0;
    for (;;) {
        int c = kbd_getc();
        if (c == '\n') {
            kputc('\n');
            break;
        } else if (c == '\b') {
            if (n > 0) { n--; kputc('\b'); }
        } else if (n < max - 1) {
            buf[n++] = (char)c;
            kputc((char)c);
        }
    }
    buf[n] = 0;
    return n;
}

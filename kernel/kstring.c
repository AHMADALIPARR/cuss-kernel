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

/* kstring.c - freestanding string/memory primitives. */
#include "kstring.h"

void *kmemset(void *d, int c, size_t n) {
    uint8_t *p = (uint8_t *)d;
    while (n--) *p++ = (uint8_t)c;
    return d;
}

void *kmemcpy(void *d, const void *s, size_t n) {
    uint8_t *dp = (uint8_t *)d;
    const uint8_t *sp = (const uint8_t *)s;
    while (n--) *dp++ = *sp++;
    return d;
}

size_t kstrlen(const char *s) {
    size_t n = 0;
    while (s[n]) n++;
    return n;
}

int kstrcmp(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return (int)(uint8_t)*a - (int)(uint8_t)*b;
}

int kstrncmp(const char *a, const char *b, size_t n) {
    while (n-- && *a && *a == *b) { a++; b++; }
    if (n == (size_t)-1) return 0;
    return (int)(uint8_t)*a - (int)(uint8_t)*b;
}

char *kstrcpy(char *d, const char *s) {
    char *r = d;
    while ((*d++ = *s++))
        ;
    return r;
}

char *kstrncpy(char *d, const char *s, size_t n) {
    char *r = d;
    while (n-- && (*d++ = *s++))
        ;
    return r;
}

/* Standard names for compiler-generated calls (freestanding provides
 * these itself; -nostdlib means no libc to fall back on). */
void *memset(void *d, int c, size_t n) { return kmemset(d, c, n); }
void *memcpy(void *d, const void *s, size_t n) { return kmemcpy(d, s, n); }

void *memmove(void *d, const void *s, size_t n) {
    uint8_t *dp = (uint8_t *)d;
    const uint8_t *sp = (const uint8_t *)s;
    if (dp < sp) {
        while (n--) *dp++ = *sp++;
    } else if (dp > sp) {
        dp += n; sp += n;
        while (n--) *--dp = *--sp;
    }
    return d;
}

int memcmp(const void *a, const void *b, size_t n) {
    const uint8_t *pa = (const uint8_t *)a, *pb = (const uint8_t *)b;
    while (n--) {
        if (*pa != *pb) return (int)*pa - (int)*pb;
        pa++; pb++;
    }
    return 0;
}

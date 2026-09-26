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

/* boot.S - multiboot1 entry. Multiboot already puts us in 32-bit
 * protected mode (flat segments, paging off, interrupts off).
 * EAX = 0x2BADB002, EBX = multiboot info pointer.
 */
.set MB_MAGIC,    0x1BADB002
.set MB_FLAGS,    0x00000000
.set MB_CHECKSUM, -(MB_MAGIC + MB_FLAGS)

.section .multiboot, "a"
.align 4
.long MB_MAGIC
.long MB_FLAGS
.long MB_CHECKSUM

.section .bss, "aw", @nobits
.align 16
stack_bottom:
.skip 16384            # 16 KiB kernel stack
stack_top:

.section .text
.global _start
.type _start, @function
_start:
    mov $stack_top, %esp
    cld
    push %ebx            # multiboot info
    push %eax            # magic
    call kmain
    cli
halt_loop:
    hlt
    jmp halt_loop

.section .note.GNU-stack,"",@progbits

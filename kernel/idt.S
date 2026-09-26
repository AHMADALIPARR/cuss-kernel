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

/* idt.S - 48 ISR stubs (32 exceptions + 16 IRQs) -> isr_handler_c. */
.macro ISR_NOERR n
.global isr_stub_\n
isr_stub_\n:
    pushl $0
    pushl $\n
    jmp isr_common
.endm

.macro ISR_ERR n
.global isr_stub_\n
isr_stub_\n:
    pushl $\n
    jmp isr_common
.endm

.section .text
isr_common:
    pusha
    movl 32(%esp), %eax     # vector
    movl 36(%esp), %ebx     # error code
    pushl %ebx
    pushl %eax
    call isr_handler_c
    addl $8, %esp
    popa
    addl $8, %esp           # drop vector + error code
    iret

# exceptions 0..31 (8,10,11,12,13,14,17,21 push an error code)
ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7
ISR_ERR   8
ISR_NOERR 9
ISR_ERR   10
ISR_ERR   11
ISR_ERR   12
ISR_ERR   13
ISR_ERR   14
ISR_NOERR 15
ISR_NOERR 16
ISR_ERR   17
ISR_NOERR 18
ISR_NOERR 19
ISR_NOERR 20
ISR_ERR   21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_NOERR 29
ISR_NOERR 30
ISR_NOERR 31
# IRQs 32..47 (PIC remapped to 0x20)
ISR_NOERR 32
ISR_NOERR 33
ISR_NOERR 34
ISR_NOERR 35
ISR_NOERR 36
ISR_NOERR 37
ISR_NOERR 38
ISR_NOERR 39
ISR_NOERR 40
ISR_NOERR 41
ISR_NOERR 42
ISR_NOERR 43
ISR_NOERR 44
ISR_NOERR 45
ISR_NOERR 46
ISR_NOERR 47

.section .data
.global isr_stubs
isr_stubs:
.long isr_stub_0,  isr_stub_1,  isr_stub_2,  isr_stub_3
.long isr_stub_4,  isr_stub_5,  isr_stub_6,  isr_stub_7
.long isr_stub_8,  isr_stub_9,  isr_stub_10, isr_stub_11
.long isr_stub_12, isr_stub_13, isr_stub_14, isr_stub_15
.long isr_stub_16, isr_stub_17, isr_stub_18, isr_stub_19
.long isr_stub_20, isr_stub_21, isr_stub_22, isr_stub_23
.long isr_stub_24, isr_stub_25, isr_stub_26, isr_stub_27
.long isr_stub_28, isr_stub_29, isr_stub_30, isr_stub_31
.long isr_stub_32, isr_stub_33, isr_stub_34, isr_stub_35
.long isr_stub_36, isr_stub_37, isr_stub_38, isr_stub_39
.long isr_stub_40, isr_stub_41, isr_stub_42, isr_stub_43
.long isr_stub_44, isr_stub_45, isr_stub_46, isr_stub_47

.section .note.GNU-stack,"",@progbits

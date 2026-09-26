# SPDX-License-Identifier: AGPL-3.0-or-later
#
# cuss-kernel
# Copyright (C) 2026 SnapKitty Collective
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU Affero General Public License as published
# by the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Affero General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.
#

# cuss-kernel Makefile
#  make          - build the 32-bit multiboot kernel ELF
#  make check    - verify multiboot header + 32-bit ELF
#  make test     - build + run the host assembler/VM test suite
#  make clean    - remove build artifacts

CC      := gcc
HOSTCC  := cc
CFLAGS  := -m32 -std=c11 -ffreestanding -fno-builtin -fno-pic -fno-pie \
           -O2 -Wall -Wextra -Werror
CPPFLAGS:= -Ikernel/include -Ikernel
ASFLAGS := -m32

KOBJS := kernel/boot.o kernel/idt_asm.o \
         kernel/kmain.o kernel/shell.o kernel/vga.o kernel/serial.o \
         kernel/kstring.o kernel/kstdio.o kernel/idt.o kernel/pic.o \
         kernel/kbd.o kernel/cuss/cuss_asm.o kernel/cuss/cuss_vm.o

.PHONY: all check test clean

all: cuss-kernel.elf

cuss-kernel.elf: $(KOBJS) kernel/linker.ld
	$(CC) -m32 -nostdlib -no-pie -T kernel/linker.ld -o $@ $(KOBJS)

kernel/idt_asm.o: kernel/idt.S
	$(CC) $(ASFLAGS) -c $< -o $@

kernel/boot.o: kernel/boot.S
	$(CC) $(ASFLAGS) -c $< -o $@

kernel/demos_gen.h: $(wildcard programs/*.cuss) tools/gen_demos.py
	python3 tools/gen_demos.py programs/*.cuss > $@

kernel/shell.o: kernel/shell.c kernel/demos_gen.h
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

kernel/%.o: kernel/%.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

check: cuss-kernel.elf
	python3 tools/check_mb.py cuss-kernel.elf

test: tests/host/harness
	tests/host/harness

tests/host/harness: tests/host/harness.c kernel/cuss/cuss_asm.c kernel/cuss/cuss_vm.c kernel/include/cuss.h
	$(HOSTCC) -std=c11 -O2 -Wall -Wextra -Werror \
	    -Ikernel/include -Ikernel \
	    tests/host/harness.c kernel/cuss/cuss_asm.c kernel/cuss/cuss_vm.c \
	    -o $@

clean:
	rm -f cuss-kernel.elf kernel/demos_gen.h tests/host/harness
	find kernel -name '*.o' -delete

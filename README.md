# cuss-kernel

A hand-rolled **C11 kernel** for x86 (32-bit, multiboot1) with a **custom
assembler embedded in it** — plus a **Mojo** host-side reference assembler
for the same ISA.

The kernel boots, sets up interrupts, runs a shell, and the shell can
assemble CUSS-1 assembly **on the bare metal** (no libc, no userspace) and
execute it on an in-kernel VM.

## The CUSS-1 ISA

See [ISA.md](ISA.md). The short version:

- 32-bit fixed-width words: `op[31:24]`, 4-bit register fields
  `rd[23:20] rs[19:16] rt[15:12]`, 16-bit immediate/address.
- 16 registers, `r0` hardwired to zero.
- 20 instructions: `nop hlt mov movi add sub mul div and or xor
  load stor jmp jz jnz out in call ret`.
- 4K-word data memory, 4M-step execution limit, 256-deep return stack.

## Layout

```
kernel/
  boot.S            multiboot1 entry, 16K stack, calls kmain
  idt.S             48 ISR stubs -> isr_handler_c
  linker.ld         linked at 0x100000, R-X / RW- segments
  kmain.c           entry: serial, VGA, IDT/PIC, keyboard, shell
  shell.c           shell: help/ver/clear/demo/demos/asm/regs
  vga.c serial.c    text-mode + COM1 drivers
  kstring.c         freestanding string/mem (+memset/memcpy for gcc)
  kstdio.c          tiny printf (%d %u %x %s %c)
  idt.c pic.c kbd.c interrupts, PIC remap, PS/2 keyboard ring buffer
  cuss/
    cuss_asm.c      two-pass CUSS-1 assembler (freestanding, shared)
    cuss_vm.c       CUSS-1 interpreter (freestanding, shared)
  include/          headers
programs/           demo .cuss programs + .expected outputs
mojo/
  cuss_asm.mojo     host-side reference assembler in Mojo
tests/host/
  harness.c         host test suite (compiles the same .c files natively)
tools/
  gen_demos.py      embeds programs/*.cuss into the kernel image
  check_mb.py       verifies the multiboot1 header in the ELF
```

`cuss_asm.c` / `cuss_vm.c` are compiled **twice**: once into the kernel
(`-m32 -ffreestanding -nostdlib`) and once natively into the host test
harness. One source of truth, tested on the host, running on bare metal.

## Build

```sh
make          # cuss-kernel.elf (32-bit multiboot kernel)
make check    # verify multiboot1 header + ELF class
make test     # host suite: assembler units, VM units, 5 demo programs
make clean
```

Requirements: `gcc` with 32-bit support, `python3`. No QEMU on the build
box, so boot is verified structurally (multiboot magic scan) rather than
by execution.

To boot: load `cuss-kernel.elf` with GRUB (multiboot1) or
`qemu-system-i386 -kernel cuss-kernel.elf`.

## Shell

```
cuss> help
cuss> demo fib        # assemble + run a built-in demo on the in-kernel VM
cuss> asm             # type assembly, empty line assembles + runs it
asm|   movi r1, 72
asm|   out r1
asm|
cuss> regs            # dump registers from the last run
```

## Demos

`programs/`: `hello` (print a string), `count` (0-9), `fib` (first 10
fibonacci numbers, exercises `call`/`ret` and a dmem digit stack),
`mem` (load/stor roundtrip), `echo` (console input).

## Mojo

`mojo/cuss_asm.mojo` is the host-side reference assembler written in Mojo
(same ISA.md contract as the in-kernel C assembler). Build with the
[Modular toolchain](https://www.modular.com/):

```sh
cd mojo && mojo build cuss_asm.mojo -o cuss_asm
./cuss_asm ../programs/fib.cuss
```

## License

AGPLv3. See LICENSE.

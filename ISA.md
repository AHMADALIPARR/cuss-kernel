# CUSS-1 ISA Specification (frozen)

CUSS = the custom assembler ISA embedded in the hand-rolled C11 kernel.
Both the in-kernel C11 assembler (`kernel/cuss/`) and the Mojo host
assembler (`mojo/`) implement exactly this spec. The VM (`cuss_vm.c`)
executes exactly this encoding.

## Word format

Fixed 32-bit words. One instruction per word.

```
 31      24 23      20 19      16 15      12 11       0
+-----------+-----------+-----------+-----------+
|  opcode   |    rd     |    rs     |    rt     |  R-type
+-----------+-----------+-----------+-----------+
|  opcode   |    rd     |    rs     |  imm16    |  I-type (rs: load/stor only)
+-----------+-----------+-----------+-----------+
|  opcode   |             addr16            |  J-type
+-----------+-------------------------------+
```

- `opcode`: 8 bits.
- `rd`, `rs`, `rt`: 4-bit register numbers (`r0`-`r15`), always valid by construction.
- `imm16` / `addr16`: 16-bit immediate or instruction-word index.

## Registers

`r0`-`r15`, 32-bit. **`r0` is hardwired zero**: reads return 0, writes are ignored.

## Instruction set

| opcode | mnemonic | form            | semantics                              |
|--------|----------|-----------------|----------------------------------------|
| 0x00   | nop      | —               | no operation                           |
| 0x01   | hlt      | —               | halt VM (exit code in r1)              |
| 0x02   | mov      | rd, rs          | rd = rs                                |
| 0x03   | movi     | rd, imm16       | rd = zero_extend(imm16)                |
| 0x04   | add      | rd, rs, rt      | rd = rs + rt                           |
| 0x05   | sub      | rd, rs, rt      | rd = rs - rt                           |
| 0x06   | mul      | rd, rs, rt      | rd = rs * rt (low 32 bits)             |
| 0x07   | div      | rd, rs, rt      | rd = rs / rt, div-by-zero -> 0xFFFFFFFF|
| 0x08   | and      | rd, rs, rt      | rd = rs & rt                           |
| 0x09   | or       | rd, rs, rt      | rd = rs \| rt                          |
| 0x0A   | xor      | rd, rs, rt      | rd = rs ^ rt                           |
| 0x0B   | load     | rd, rs, imm16   | rd = dmem[rs + imm16]                  |
| 0x0C   | stor     | rd, rs, imm16   | dmem[rs + imm16] = rd                  |
| 0x0D   | jmp      | addr16          | pc = addr                              |
| 0x0E   | jz       | rd, addr16      | if rd == 0: pc = addr                  |
| 0x0F   | jnz      | rd, addr16      | if rd != 0: pc = addr                  |
| 0x10   | out      | rd              | emit low byte of rd to console         |
| 0x11   | in       | rd              | rd = next input byte (0 if none)       |
| 0x12   | call     | addr16          | push pc+1; pc = addr                   |
| 0x13   | ret      | —               | pc = pop(); empty stack -> halt(error) |

- `addr16`: instruction-word index (label or absolute).
- `dmem`: 4096 words of VM data memory, word-addressed, zero-initialised.
- `in`/`out`: the VM's console hooks (kernel: keyboard/VGA; host tests: buffers).
- Execution step limit: 4,000,000 steps, then halt with `CUSS_ERR_STEPS`.
- Unknown opcode: halt with `CUSS_ERR_OPCODE`.
- `ret` on empty return stack: halt with `CUSS_ERR_RET`.

## Assembly syntax (both assemblers)

```
; comment runs to end of line
label:                 ; defines label = current word index
    movi r1, 65        ; mnemonics case-insensitive, regs r0-r15
    add  r3, r1, r2
    jz   r3, done
    jmp  loop
done:
    hlt
```

- Labels may be used anywhere an `imm16`/`addr16` is expected.
- Two passes: pass 1 records labels, pass 2 emits words.
- Errors: unknown mnemonic, bad register, undefined label, operand count
  mismatch, immediate out of range (0..65535), duplicate label.

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

/* cuss.h - CUSS-1 ISA contract shared by the kernel, host tests, and docs.
 * Implements ISA.md. Freestanding-safe (only <stdint.h>).
 */
#ifndef CUSS_H
#define CUSS_H

#include <stdint.h>

/* ---- opcodes ---- */
#define CUSS_OP_NOP  0x00
#define CUSS_OP_HLT  0x01
#define CUSS_OP_MOV  0x02
#define CUSS_OP_MOVI 0x03
#define CUSS_OP_ADD  0x04
#define CUSS_OP_SUB  0x05
#define CUSS_OP_MUL  0x06
#define CUSS_OP_DIV  0x07
#define CUSS_OP_AND  0x08
#define CUSS_OP_OR   0x09
#define CUSS_OP_XOR  0x0A
#define CUSS_OP_LOAD 0x0B
#define CUSS_OP_STOR 0x0C
#define CUSS_OP_JMP  0x0D
#define CUSS_OP_JZ   0x0E
#define CUSS_OP_JNZ  0x0F
#define CUSS_OP_OUT  0x10
#define CUSS_OP_IN   0x11
#define CUSS_OP_CALL 0x12
#define CUSS_OP_RET  0x13

/* ---- VM limits ---- */
#define CUSS_NREGS      16
#define CUSS_DMEM_WORDS 4096
#define CUSS_MAX_STEPS  4000000u
#define CUSS_RSTACK     256

/* ---- errors ---- */
typedef enum {
    CUSS_OK = 0,
    CUSS_ERR_OPCODE,   /* unknown opcode word */
    CUSS_ERR_STEPS,    /* step limit exceeded (likely infinite loop) */
    CUSS_ERR_RET,      /* ret with empty return stack */
    CUSS_ERR_ASM,      /* assembly failed; see errbuf */
    CUSS_ERR_MEM       /* data-memory address out of range */
} cuss_err_t;

/* ---- assembler ---- */
#define CUSS_MAX_LABELS 512
#define CUSS_MAX_LINE   256

/* Assemble NUL-terminated source into out_words.
 * Returns 0 on success; -1 on error with a message in errbuf. */
int cuss_assemble(const char *src, uint32_t *out_words, uint32_t max_words,
                  uint32_t *out_nwords, char *errbuf, uint32_t errbuf_len);

/* ---- VM ---- */
typedef struct {
    void (*putc)(char c, void *ctx);
    int (*getc)(void *ctx);   /* -1 when no input available */
    void *ctx;
} cuss_io_t;

typedef struct {
    uint32_t regs[CUSS_NREGS];
    uint32_t pc;
    uint32_t steps;
    uint32_t exit_code;   /* r1 at hlt */
} cuss_vm_t;

cuss_err_t cuss_vm_run(const uint32_t *prog, uint32_t nwords,
                       const cuss_io_t *io, cuss_vm_t *state);

const char *cuss_errstr(cuss_err_t e);

#endif /* CUSS_H */

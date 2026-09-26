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

/* cuss_vm.c - interpreter for the CUSS-1 ISA (see ISA.md).
 *
 * Fully freestanding. The same file is compiled into the kernel AND into
 * the host test harness. Console I/O goes through the cuss_io_t hooks so
 * the kernel (VGA/keyboard) and the host harness (buffers) differ only
 * in the hooks they install.
 */
#include "cuss.h"

const char *cuss_errstr(cuss_err_t e) {
    switch (e) {
    case CUSS_OK:        return "ok";
    case CUSS_ERR_OPCODE: return "bad opcode / jump out of program";
    case CUSS_ERR_STEPS:  return "step limit exceeded";
    case CUSS_ERR_RET:    return "return-stack underflow/overflow";
    case CUSS_ERR_ASM:    return "assembly failed";
    case CUSS_ERR_MEM:    return "data memory address out of range";
    default:              return "unknown error";
    }
}

cuss_err_t cuss_vm_run(const uint32_t *prog, uint32_t nwords,
                       const cuss_io_t *io, cuss_vm_t *state) {
    static uint32_t dmem[CUSS_DMEM_WORDS]; /* static: keeps kernel stack small */
    uint32_t regs[CUSS_NREGS];
    uint32_t rstack[CUSS_RSTACK];
    uint32_t pc = 0, steps = 0, sp = 0;
    cuss_err_t err = CUSS_OK;

    for (uint32_t i = 0; i < CUSS_NREGS; i++) regs[i] = 0;
    for (uint32_t i = 0; i < CUSS_DMEM_WORDS; i++) dmem[i] = 0;

    /* helper macros: 4-bit reg fields are always < 16 by construction */
#define RD  ((w >> 20) & 15u)
#define RS  ((w >> 16) & 15u)
#define RT  ((w >> 12) & 15u)
#define IMM (w & 0xFFFFu)

    while (1) {
        if (steps++ >= CUSS_MAX_STEPS) { err = CUSS_ERR_STEPS; break; }
        if (pc >= nwords) { err = CUSS_ERR_OPCODE; break; }
        uint32_t w = prog[pc];
        uint8_t op = (uint8_t)(w >> 24);
        uint32_t npc = pc + 1;

        switch (op) {
        case CUSS_OP_NOP: break;
        case CUSS_OP_HLT:
            goto done;
        case CUSS_OP_MOV:  regs[RD] = regs[RS]; break;
        case CUSS_OP_MOVI: regs[RD] = IMM; break;
        case CUSS_OP_ADD:  regs[RD] = regs[RS] + regs[RT]; break;
        case CUSS_OP_SUB:  regs[RD] = regs[RS] - regs[RT]; break;
        case CUSS_OP_MUL:  regs[RD] = regs[RS] * regs[RT]; break;
        case CUSS_OP_DIV:
            regs[RD] = regs[RT] ? regs[RS] / regs[RT] : 0xFFFFFFFFu;
            break;
        case CUSS_OP_AND:  regs[RD] = regs[RS] & regs[RT]; break;
        case CUSS_OP_OR:   regs[RD] = regs[RS] | regs[RT]; break;
        case CUSS_OP_XOR:  regs[RD] = regs[RS] ^ regs[RT]; break;
        case CUSS_OP_LOAD: {
            uint32_t a = regs[RS] + IMM;
            if (a >= CUSS_DMEM_WORDS) { err = CUSS_ERR_MEM; goto done; }
            regs[RD] = dmem[a];
            break;
        }
        case CUSS_OP_STOR: {
            uint32_t a = regs[RS] + IMM;
            if (a >= CUSS_DMEM_WORDS) { err = CUSS_ERR_MEM; goto done; }
            dmem[a] = regs[RD];
            break;
        }
        case CUSS_OP_JMP:
            if (IMM >= nwords) { err = CUSS_ERR_OPCODE; goto done; }
            npc = IMM;
            break;
        case CUSS_OP_JZ:
            if (regs[RD] == 0) {
                if (IMM >= nwords) { err = CUSS_ERR_OPCODE; goto done; }
                npc = IMM;
            }
            break;
        case CUSS_OP_JNZ:
            if (regs[RD] != 0) {
                if (IMM >= nwords) { err = CUSS_ERR_OPCODE; goto done; }
                npc = IMM;
            }
            break;
        case CUSS_OP_OUT:
            if (io && io->putc) io->putc((char)(regs[RD] & 0xFF), io->ctx);
            break;
        case CUSS_OP_IN: {
            int c = (io && io->getc) ? io->getc(io->ctx) : -1;
            regs[RD] = c < 0 ? 0 : (uint32_t)(c & 0xFF);
            break;
        }
        case CUSS_OP_CALL:
            if (IMM >= nwords || sp >= CUSS_RSTACK) {
                err = (sp >= CUSS_RSTACK) ? CUSS_ERR_RET : CUSS_ERR_OPCODE;
                goto done;
            }
            rstack[sp++] = npc;
            npc = IMM;
            break;
        case CUSS_OP_RET:
            if (sp == 0) { err = CUSS_ERR_RET; goto done; }
            npc = rstack[--sp];
            break;
        default:
            err = CUSS_ERR_OPCODE;
            goto done;
        }

        regs[0] = 0; /* r0 hardwired zero */
        pc = npc;
    }

done:
    if (state) {
        for (uint32_t i = 0; i < CUSS_NREGS; i++) state->regs[i] = regs[i];
        state->regs[0] = 0;
        state->pc = pc;
        state->steps = steps;
        state->exit_code = regs[1];
    }
    return err;

#undef RD
#undef RS
#undef RT
#undef IMM
}

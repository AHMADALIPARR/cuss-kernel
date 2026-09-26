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

/* cuss_asm.c - two-pass assembler for the CUSS-1 ISA (see ISA.md).
 *
 * Fully freestanding: depends only on <stdint.h> and cuss.h. The same file
 * is compiled into the kernel AND into the host test harness.
 */
#include "cuss.h"

#define MAX_TOKENS   8
#define MAX_LABEL_LEN 48

/* ---------- tiny private string helpers (no libc) ---------- */
static uint32_t s_len(const char *s) {
    uint32_t n = 0;
    while (s[n]) n++;
    return n;
}
static char s_up(char c) { return (c >= 'a' && c <= 'z') ? (char)(c - 32) : c; }
static int s_eq(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}
static int s_eqci(const char *a, const char *b) {
    while (*a && s_up(*a) == s_up(*b)) { a++; b++; }
    return s_up(*a) == s_up(*b);
}
static int s_isspace(char c) {
    return c == ' ' || c == '\t' || c == '\r';
}
/* unsigned decimal/hex parse; returns 1 ok. hex if 0x prefix. */
static int s_tou32(const char *s, uint32_t *out) {
    uint32_t v = 0;
    int base = 10, any = 0;
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) { base = 16; s += 2; }
    while (*s) {
        uint32_t d;
        if (*s >= '0' && *s <= '9') d = (uint32_t)(*s - '0');
        else if (base == 16 && *s >= 'a' && *s <= 'f') d = (uint32_t)(*s - 'a' + 10);
        else if (base == 16 && *s >= 'A' && *s <= 'F') d = (uint32_t)(*s - 'A' + 10);
        else return 0;
        if (d >= (uint32_t)base) return 0;
        v = v * (uint32_t)base + d;
        any = 1;
        s++;
    }
    if (!any) return 0;
    *out = v;
    return 1;
}
static void u2s(uint32_t v, char *buf) {
    char tmp[12];
    int i = 0;
    if (v == 0) { buf[0] = '0'; buf[1] = 0; return; }
    while (v) { tmp[i++] = (char)('0' + v % 10); v /= 10; }
    int j = 0;
    while (i) buf[j++] = tmp[--i];
    buf[j] = 0;
}

/* ---------- mnemonic table ---------- */
typedef struct { const char *name; uint8_t op; uint8_t nops; } mop_t;
static const mop_t MOPS[] = {
    {"nop",  CUSS_OP_NOP,  0},
    {"hlt",  CUSS_OP_HLT,  0},
    {"ret",  CUSS_OP_RET,  0},
    {"mov",  CUSS_OP_MOV,  2},
    {"movi", CUSS_OP_MOVI, 2},
    {"add",  CUSS_OP_ADD,  3},
    {"sub",  CUSS_OP_SUB,  3},
    {"mul",  CUSS_OP_MUL,  3},
    {"div",  CUSS_OP_DIV,  3},
    {"and",  CUSS_OP_AND,  3},
    {"or",   CUSS_OP_OR,   3},
    {"xor",  CUSS_OP_XOR,  3},
    {"load", CUSS_OP_LOAD, 3},
    {"stor", CUSS_OP_STOR, 3},
    {"jmp",  CUSS_OP_JMP,  1},
    {"call", CUSS_OP_CALL, 1},
    {"jz",   CUSS_OP_JZ,   2},
    {"jnz",  CUSS_OP_JNZ,  2},
    {"out",  CUSS_OP_OUT,  1},
    {"in",   CUSS_OP_IN,   1},
};
#define NMOPS (sizeof(MOPS) / sizeof(MOPS[0]))

/* ---------- label table ---------- */
typedef struct { char name[MAX_LABEL_LEN + 1]; uint32_t addr; } label_t;
typedef struct {
    label_t items[CUSS_MAX_LABELS];
    uint32_t n;
} labels_t;

static int labels_add(labels_t *l, const char *name, uint32_t addr) {
    if (l->n >= CUSS_MAX_LABELS) return -1;
    uint32_t i = 0;
    while (name[i] && i < MAX_LABEL_LEN) { l->items[l->n].name[i] = name[i]; i++; }
    l->items[l->n].name[i] = 0;
    l->items[l->n].addr = addr;
    l->n++;
    return 0;
}
static int labels_find(const labels_t *l, const char *name, uint32_t *addr) {
    for (uint32_t i = 0; i < l->n; i++)
        if (s_eq(l->items[i].name, name)) { *addr = l->items[i].addr; return 0; }
    return -1;
}

/* ---------- error reporting ---------- */
typedef struct {
    char *buf;
    uint32_t len;
    int failed;
} err_t;

static void efail(err_t *e, uint32_t lineno, const char *msg, const char *extra) {
    char ln[12];
    u2s(lineno, ln);
    uint32_t p = 0;
    const char *parts[] = { "line ", ln, ": ", msg, extra, 0 };
    for (int i = 0; parts[i] && p + 1 < e->len; i++) {
        const char *s = parts[i];
        while (*s && p + 1 < e->len) e->buf[p++] = *s++;
    }
    e->buf[p] = 0;
    e->failed = 1;
}

/* ---------- operand parsing ---------- */
static int parse_reg(const char *t, uint8_t *reg) {
    if ((t[0] != 'r' && t[0] != 'R') || !t[1]) return -1;
    uint32_t v;
    if (!s_tou32(t + 1, &v) || v > 15) return -1;
    *reg = (uint8_t)v;
    return 0;
}
/* immediate: number or label (labels resolved in pass 2) */
static int parse_imm(const char *t, const labels_t *l, int resolving,
                     uint16_t *imm) {
    uint32_t v;
    if (s_tou32(t, &v)) {
        if (v > 0xFFFF) return -1;
        *imm = (uint16_t)v;
        return 0;
    }
    if (resolving) {
        uint32_t addr;
        if (labels_find(l, t, &addr) == 0 && addr <= 0xFFFF) {
            *imm = (uint16_t)addr;
            return 0;
        }
        return -1;
    }
    *imm = 0;
    return 0; /* pass 1: labels not yet known, accept tentatively */
}

/* ---------- encoders ----------
 * R-type: op[31:24] rd[23:20] rs[19:16] rt[15:12]
 * I-type: op[31:24] rd[23:20] rs[19:16] imm[15:0]   (rs only for load/stor)
 * J-type: op[31:24] addr[15:0]
 */
static uint32_t enc_r(uint8_t op, uint8_t rd, uint8_t rs, uint8_t rt) {
    return ((uint32_t)op << 24) | ((uint32_t)(rd & 15) << 20) |
           ((uint32_t)(rs & 15) << 16) | ((uint32_t)(rt & 15) << 12);
}
static uint32_t enc_i(uint8_t op, uint8_t rd, uint16_t imm) {
    return ((uint32_t)op << 24) | ((uint32_t)(rd & 15) << 20) | imm;
}
static uint32_t enc_ri(uint8_t op, uint8_t rd, uint8_t rs, uint16_t imm) {
    return ((uint32_t)op << 24) | ((uint32_t)(rd & 15) << 20) |
           ((uint32_t)(rs & 15) << 16) | imm;
}
static uint32_t enc_j(uint8_t op, uint16_t addr) {
    return ((uint32_t)op << 24) | addr;
}

/* ---------- line handling ---------- */
/* Split one raw line (NUL-terminated, writable) into tokens.
 * Strips comments, handles `label:` prefix. Returns token count;
 * *is_label_only set when the line was just a label. */
static int tokenize_line(char *line, char *tok[MAX_TOKENS],
                         const char **label, err_t *e, uint32_t lineno) {
    *label = 0;
    for (char *c = line; *c; c++)
        if (*c == ';') { *c = 0; break; }
    int n = 0;
    char *p = line;
    while (*p && n < MAX_TOKENS) {
        while (s_isspace(*p) || *p == ',') p++;
        if (!*p) break;
        tok[n++] = p;
        while (*p && !s_isspace(*p) && *p != ',') p++;
        if (*p) *p++ = 0;
    }
    if (n == 0) return 0;
    /* label? `name:` */
    uint32_t tl = s_len(tok[0]);
    if (tl > 1 && tok[0][tl - 1] == ':') {
        tok[0][tl - 1] = 0;
        if (s_len(tok[0]) == 0 || s_len(tok[0]) > MAX_LABEL_LEN) {
            efail(e, lineno, "bad label name", "");
            return -1;
        }
        *label = tok[0];
        for (int i = 0; i + 1 < n; i++) tok[i] = tok[i + 1];
        n--;
    }
    return n;
}

static const mop_t *find_mop(const char *name) {
    for (uint32_t i = 0; i < NMOPS; i++)
        if (s_eqci(MOPS[i].name, name)) return &MOPS[i];
    return 0;
}

/* ---------- the assembler ---------- */
int cuss_assemble(const char *src, uint32_t *out_words, uint32_t max_words,
                  uint32_t *out_nwords, char *errbuf, uint32_t errbuf_len) {
    err_t e = { errbuf, errbuf_len, 0 };
    if (errbuf && errbuf_len) errbuf[0] = 0;
    labels_t labels = { 0 };

    /* pass 1: collect labels, count words */
    {
        uint32_t addr = 0, lineno = 0;
        const char *p = src;
        char line[CUSS_MAX_LINE];
        while (*p && !e.failed) {
            lineno++;
            uint32_t i = 0;
            while (*p && *p != '\n' && i + 1 < sizeof(line)) line[i++] = *p++;
            if (*p == '\n') p++;
            line[i] = 0;
            char *tok[MAX_TOKENS];
            const char *label;
            int n = tokenize_line(line, tok, &label, &e, lineno);
            if (n < 0) break;
            if (label) {
                uint32_t dummy;
                if (labels_find(&labels, label, &dummy) == 0) {
                    efail(&e, lineno, "duplicate label: ", label);
                    break;
                }
                if (labels_add(&labels, label, addr) != 0) {
                    efail(&e, lineno, "too many labels", "");
                    break;
                }
            }
            if (n > 0) {
                if (!find_mop(tok[0])) {
                    efail(&e, lineno, "unknown mnemonic: ", tok[0]);
                    break;
                }
                addr++;
            }
        }
        if (e.failed) return -1;
    }

    /* pass 2: encode */
    {
        uint32_t addr = 0, lineno = 0;
        const char *p = src;
        char line[CUSS_MAX_LINE];
        while (*p && !e.failed) {
            lineno++;
            uint32_t i = 0;
            while (*p && *p != '\n' && i + 1 < sizeof(line)) line[i++] = *p++;
            if (*p == '\n') p++;
            line[i] = 0;
            char *tok[MAX_TOKENS];
            const char *label;
            int n = tokenize_line(line, tok, &label, &e, lineno);
            if (n < 0) break;
            if (n == 0) continue;
            const mop_t *m = find_mop(tok[0]);
            if (!m) { efail(&e, lineno, "unknown mnemonic: ", tok[0]); break; }
            if (n - 1 != m->nops) {
                efail(&e, lineno, "wrong operand count for: ", tok[0]);
                break;
            }
            if (addr >= max_words) {
                efail(&e, lineno, "program too large", "");
                break;
            }
            uint8_t r1 = 0, r2 = 0, r3 = 0;
            uint16_t imm = 0;
            int bad = 0;
            /* operand decode per opcode shape */
            switch (m->op) {
            case CUSS_OP_MOV:
                bad = parse_reg(tok[1], &r1) || parse_reg(tok[2], &r2);
                out_words[addr] = enc_r(m->op, r1, r2, 0);
                break;
            case CUSS_OP_MOVI:
                bad = parse_reg(tok[1], &r1) ||
                      parse_imm(tok[2], &labels, 1, &imm);
                out_words[addr] = enc_i(m->op, r1, imm);
                break;
            case CUSS_OP_ADD: case CUSS_OP_SUB: case CUSS_OP_MUL:
            case CUSS_OP_DIV: case CUSS_OP_AND: case CUSS_OP_OR:
            case CUSS_OP_XOR:
                bad = parse_reg(tok[1], &r1) || parse_reg(tok[2], &r2) ||
                      parse_reg(tok[3], &r3);
                out_words[addr] = enc_r(m->op, r1, r2, r3);
                break;
            case CUSS_OP_LOAD: case CUSS_OP_STOR:
                bad = parse_reg(tok[1], &r1) || parse_reg(tok[2], &r2) ||
                      parse_imm(tok[3], &labels, 1, &imm);
                out_words[addr] = enc_ri(m->op, r1, r2, imm);
                break;
            case CUSS_OP_JMP: case CUSS_OP_CALL:
                bad = parse_imm(tok[1], &labels, 1, &imm);
                out_words[addr] = enc_j(m->op, imm);
                break;
            case CUSS_OP_JZ: case CUSS_OP_JNZ:
                bad = parse_reg(tok[1], &r1) ||
                      parse_imm(tok[2], &labels, 1, &imm);
                out_words[addr] = enc_i(m->op, r1, imm);
                break;
            case CUSS_OP_OUT: case CUSS_OP_IN:
                bad = parse_reg(tok[1], &r1);
                out_words[addr] = enc_i(m->op, r1, 0);
                break;
            default: /* nop, hlt, ret */
                out_words[addr] = enc_j(m->op, 0);
                break;
            }
            if (bad) {
                efail(&e, lineno, "bad operand for: ", tok[0]);
                break;
            }
            addr++;
        }
        if (e.failed) return -1;
        *out_nwords = addr;
    }
    return 0;
}

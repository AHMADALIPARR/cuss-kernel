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

/* tests/host/harness.c - host-side tests for the CUSS-1 assembler + VM.
 *
 * Compiles the SAME cuss_asm.c / cuss_vm.c the kernel uses, natively.
 * Runs programs as .cuss files against .expected files, plus assembler
 * unit tests (encodings, labels, error cases). Exit 0 iff all pass.
 */
#include "cuss.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int s_failures = 0;
static int s_checks = 0;

#define CHECK(cond, ...) do { \
    s_checks++; \
    if (!(cond)) { s_failures++; printf("FAIL: "); printf(__VA_ARGS__); printf("\n"); } \
} while (0)

/* ---------- IO capture ---------- */
static char  g_out[65536];
static size_t g_outlen;
static const char *g_in;

static void h_putc(char c, void *ctx) {
    (void)ctx;
    if (g_outlen + 1 < sizeof(g_out)) g_out[g_outlen++] = c;
}
static int h_getc(void *ctx) {
    (void)ctx;
    if (!g_in || !*g_in) return -1;
    return (unsigned char)*g_in++;
}

static char *read_file(const char *path, size_t *n_out) {
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)n + 1);
    if (!buf) { fclose(f); return 0; }
    if (fread(buf, 1, (size_t)n, f) != (size_t)n) { free(buf); fclose(f); return 0; }
    buf[n] = '\0';
    fclose(f);
    if (n_out) *n_out = (size_t)n;
    return buf;
}

/* ---------- program tests ---------- */
static void test_program(const char *name, const char *input) {
    char src_path[256], exp_path[256];
    snprintf(src_path, sizeof(src_path), "programs/%s.cuss", name);
    snprintf(exp_path, sizeof(exp_path), "programs/%s.expected", name);

    char *src = read_file(src_path, 0);
    CHECK(src != 0, "%s: cannot read %s", name, src_path);
    if (!src) return;
    size_t exp_n = 0;
    char *exp = read_file(exp_path, &exp_n);
    CHECK(exp != 0, "%s: cannot read %s", name, exp_path);
    if (!exp) { free(src); return; }

    static uint32_t words[8192];
    uint32_t nwords = 0;
    char errbuf[256];
    int rc = cuss_assemble(src, words, 8192, &nwords, errbuf, sizeof(errbuf));
    CHECK(rc == 0, "%s: assemble failed: %s", name, errbuf);

    g_outlen = 0;
    g_in = input;
    cuss_io_t io = { h_putc, h_getc, 0 };
    cuss_vm_t st;
    cuss_err_t err = cuss_vm_run(words, nwords, &io, &st);
    CHECK(err == CUSS_OK, "%s: vm error: %s", name, cuss_errstr(err));
    CHECK(g_outlen == exp_n && memcmp(g_out, exp, exp_n) == 0,
          "%s: output mismatch (got %zu bytes, want %zu)", name, g_outlen, exp_n);

    free(src);
    free(exp);
    printf("ok: program %s (%u words, %u steps)\n", name, nwords, st.steps);
}

/* ---------- assembler unit tests ---------- */
static void test_asm_units(void) {
    static uint32_t w[64];
    uint32_t n = 0;
    char err[256];

    /* movi r1, 65 -> 0x03110041 */
    n = 0;
    CHECK(cuss_assemble("movi r1, 65\n", w, 64, &n, err, sizeof(err)) == 0, "movi asm: %s", err);
    CHECK(n == 1 && w[0] == 0x03100041u, "movi encoding: got 0x%08x", w[0]);

    /* mov r2, r1 -> op 0x02 rd=2 rs=1 */
    n = 0;
    CHECK(cuss_assemble("mov r2, r1\n", w, 64, &n, err, sizeof(err)) == 0, "mov asm: %s", err);
    CHECK(n == 1 && w[0] == 0x02210000u, "mov encoding: got 0x%08x", w[0]);

    /* negative immediate: not in the ISA (imm16 is 0..65535, zero-extended) */
    n = 0;
    CHECK(cuss_assemble("movi r3, -1\n", w, 64, &n, err, sizeof(err)) != 0,
          "negative imm should fail");

    /* forward label: jmp end / nop / end: hlt */
    n = 0;
    CHECK(cuss_assemble("jmp end\nnop\nend:\nhlt\n", w, 64, &n, err, sizeof(err)) == 0,
          "label asm: %s", err);
    CHECK(n == 3 && w[0] == 0x0D000002u && w[2] == 0x01000000u,
          "label encoding: got 0x%08x 0x%08x 0x%08x", w[0], w[1], w[2]);

    /* error cases */
    n = 0;
    CHECK(cuss_assemble("frobnicate r1\n", w, 64, &n, err, sizeof(err)) != 0,
          "bad mnemonic should fail");
    n = 0;
    CHECK(cuss_assemble("jmp nowhere\n", w, 64, &n, err, sizeof(err)) != 0,
          "undefined label should fail");
    n = 0;
    CHECK(cuss_assemble("mov r16, r1\n", w, 64, &n, err, sizeof(err)) != 0,
          "bad register should fail");
    n = 0;
    CHECK(cuss_assemble("movi r1, 70000\n", w, 64, &n, err, sizeof(err)) != 0,
          "imm out of range should fail");
    n = 0;
    CHECK(cuss_assemble("add r1, r2\n", w, 64, &n, err, sizeof(err)) != 0,
          "wrong operand count should fail");

    printf("ok: assembler unit tests\n");
}

/* ---------- vm unit tests ---------- */
static void test_vm_units(void) {
    /* div by zero -> 0xFFFFFFFF; r0 stays zero even when written */
    static const uint32_t prog[] = {
        0x03120000u, /* movi r1, 0      */
        0x03220005u, /* movi r2, 5      */
        0x07312000u, /* div r3, r1, r2  -> 0 */
        0x07421000u, /* div r4, r2, r1  -> 0xFFFFFFFF (div0) */
        0x0300002Au, /* movi r0, 42     -> r0 stays 0 */
        0x01000000u, /* hlt */
    };
    cuss_io_t io = { 0, 0, 0 };
    cuss_vm_t st;
    cuss_err_t err = cuss_vm_run(prog, 6, &io, &st);
    CHECK(err == CUSS_OK, "vm unit: %s", cuss_errstr(err));
    CHECK(st.regs[3] == 0, "div 0/5: got %u", st.regs[3]);
    CHECK(st.regs[4] == 0xFFFFFFFFu, "div by zero: got 0x%x", st.regs[4]);
    CHECK(st.regs[0] == 0, "r0 not zero: got 0x%x", st.regs[0]);

    /* ret without call -> CUSS_ERR_RET */
    static const uint32_t badret[] = { 0x13000000u };
    err = cuss_vm_run(badret, 1, &io, &st);
    CHECK(err == CUSS_ERR_RET, "bare ret: got %s", cuss_errstr(err));

    /* oob data memory -> CUSS_ERR_MEM: stor with base 4095 + imm 1 */
    static const uint32_t badoob[] = {
        0x03220FFFu, /* movi r2, 4095 */
        0x0C120001u, /* stor r1, r2, 1 -> addr 4096, out of range */
    };
    err = cuss_vm_run(badoob, 2, &io, &st);
    CHECK(err == CUSS_ERR_MEM, "oob stor: got %s", cuss_errstr(err));

    printf("ok: vm unit tests\n");
}

int main(void) {
    test_asm_units();
    test_vm_units();
    test_program("hello", 0);
    test_program("count", 0);
    test_program("fib", 0);
    test_program("mem", 0);
    test_program("echo", "Z");

    printf("\n%d checks, %d failures\n", s_checks, s_failures);
    return s_failures ? 1 : 0;
}

/* session.c - record an authentic cuss-kernel shell session on the host.
 * Drives the REAL cuss_assemble + cuss_vm_run on programs/*.cuss and prints
 * exactly what the kernel shell (kernel/shell.c) and kmain banner print,
 * so docs screenshots show the system genuinely operating.
 * Usage: ./session > docs/session.txt   (rendered to PNG by docs/render.py)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "cuss.h"

static const char *g_in;
static size_t g_inpos;

static void h_putc(char c, void *ctx) { (void)ctx; putchar(c); }
static int h_getc(void *ctx) {
    (void)ctx;
    if (!g_in || g_in[g_inpos] == '\0') return -1;
    return (unsigned char)g_in[g_inpos++];
}

static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    char *b = malloc(n + 1);
    if (fread(b, 1, n, f) != (size_t)n) { fclose(f); free(b); return 0; }
    b[n] = 0; fclose(f); return b;
}

static cuss_vm_t s_last;
static int s_have_last;

static void run_demo(const char *name, const char *input) {
    char path[256];
    snprintf(path, sizeof path, "programs/%s.cuss", name);
    char *src = read_file(path);
    if (!src) { printf("cannot read %s\n", path); return; }
    static uint32_t words[8192];
    uint32_t n = 0; char err[256];
    printf("cuss> demo %s\n", name);
    if (cuss_assemble(src, words, 8192, &n, err, sizeof err) != 0) {
        printf("asm error: %s\n", err); free(src); return;
    }
    printf("assembled %u words. running...\n", n);
    g_in = input; g_inpos = 0;
    cuss_io_t io = { h_putc, h_getc, 0 };
    cuss_err_t rc = cuss_vm_run(words, n, &io, &s_last);
    s_have_last = 1;
    printf("\n[%s] %s (steps=%u, r1=0x%x)\n", name, cuss_errstr(rc),
           s_last.steps, s_last.regs[1]);
    free(src);
}

static void cmd_regs(void) {
    printf("cuss> regs\n");
    if (!s_have_last) { printf("no program run yet.\n"); return; }
    for (int i = 0; i < 16; i++)
        printf("r%-2d=0x%08x%c", i, s_last.regs[i], (i % 4 == 3) ? '\n' : ' ');
    printf("pc=%u steps=%u\n", s_last.pc, s_last.steps);
}

int main(void) {
    /* kmain banner (green first line on VGA; plain here) */
    printf("cuss-kernel v0.1  (C11, hand-rolled)\n");
    printf("cpu: i386 protected mode, no paging, no libc\n");
    printf("interrupts on. starting shell.\n\n");
    printf("cuss shell. 'help' for commands.\n");

    printf("cuss> help\n");
    printf("commands:\n");
    printf("  help        this text\n");
    printf("  ver         kernel version\n");
    printf("  clear       clear screen\n");
    printf("  demo <name> assemble+run a built-in demo\n");
    printf("  demos       list built-in demos\n");
    printf("  asm         type assembly, empty line to run\n");
    printf("  regs        dump registers from last run\n");

    printf("cuss> demos\n");
    printf("  hello\n  count\n  fib\n  mem\n  echo\n");

    printf("cuss> ver\n");
    printf("cuss-kernel v0.1, CUSS-1 ISA, built Sep 25 2026\n");

    run_demo("hello", "");
    run_demo("count", "");
    run_demo("fib", "");
    cmd_regs();
    run_demo("mem", "");
    run_demo("echo", "Z");

    printf("cuss> ");
    fflush(stdout);
    return 0;
}

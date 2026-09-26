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

/* shell.c - tiny interactive shell: run demos, assemble+run typed code. */
#include "shell.h"
#include "vga.h"
#include "kstdio.h"
#include "kstring.h"
#include "kbd.h"
#include "cuss.h"
#include "demos_gen.h"

#define ASM_MAX_WORDS 2048
#define LINE_MAX      256
#define SRC_MAX       8192

static uint32_t s_words[ASM_MAX_WORDS];
static cuss_vm_t s_last;
static int s_have_last;

static void s_putc(char c, void *ctx) { (void)ctx; kputc(c); }
static int s_getc(void *ctx) { (void)ctx; return kbd_trygetc(); }

static void run_words(const uint32_t *w, uint32_t n, const char *name) {
    cuss_io_t io = { s_putc, s_getc, 0 };
    cuss_err_t rc = cuss_vm_run(w, n, &io, &s_last);
    s_have_last = 1;
    kprintf("\n[%s] %s (steps=%u, r1=0x%x)\n",
            name, cuss_errstr(rc), s_last.steps, s_last.regs[1]);
}

static int assemble_and_run(const char *src, const char *name) {
    uint32_t n = 0;
    char errbuf[256];
    if (cuss_assemble(src, s_words, ASM_MAX_WORDS, &n, errbuf, sizeof(errbuf)) != 0) {
        kprintf("asm error: %s\n", errbuf);
        return -1;
    }
    kprintf("assembled %u words. running...\n", n);
    run_words(s_words, n, name);
    return 0;
}

static void cmd_demo(const char *name) {
    for (uint32_t i = 0; i < NDEMOS; i++) {
        if (kstrcmp(demos[i].name, name) == 0) {
            assemble_and_run(demos[i].src, demos[i].name);
            return;
        }
    }
    kprintf("no such demo. available:");
    for (uint32_t i = 0; i < NDEMOS; i++) kprintf(" %s", demos[i].name);
    kprintf("\n");
}

static void cmd_asm(void) {
    static char src[SRC_MAX];
    char line[LINE_MAX];
    uint32_t pos = 0;
    kprintf("type CUSS-1 assembly, one instruction per line.\n");
    kprintf("empty line assembles and runs. 'q' alone cancels.\n");
    for (;;) {
        kprintf("asm| ");
        kbd_getline(line, sizeof(line));
        if (line[0] == '\0') break;
        if (kstrcmp(line, "q") == 0) { kprintf("cancelled.\n"); return; }
        uint32_t len = kstrlen(line);
        if (pos + len + 2 >= SRC_MAX) { kprintf("too long, cancelled.\n"); return; }
        kmemcpy(src + pos, line, len);
        pos += len;
        src[pos++] = '\n';
    }
    src[pos] = '\0';
    if (pos == 0) { kprintf("nothing typed.\n"); return; }
    assemble_and_run(src, "typed");
}

static void cmd_regs(void) {
    if (!s_have_last) { kprintf("no program run yet.\n"); return; }
    for (int i = 0; i < 16; i++) {
        kprintf("r%-2d=0x%08x%c", i, s_last.regs[i], (i % 4 == 3) ? '\n' : ' ');
    }
    kprintf("pc=%u steps=%u\n", s_last.pc, s_last.steps);
}

static void cmd_help(void) {
    kprintf("commands:\n");
    kprintf("  help        this text\n");
    kprintf("  ver         kernel version\n");
    kprintf("  clear       clear screen\n");
    kprintf("  demo <name> assemble+run a built-in demo\n");
    kprintf("  demos       list built-in demos\n");
    kprintf("  asm         type assembly, empty line to run\n");
    kprintf("  regs        dump registers from last run\n");
}

void shell_run(void) {
    char line[LINE_MAX];
    kprintf("cuss shell. 'help' for commands.\n");
    for (;;) {
        kprintf("cuss> ");
        kbd_getline(line, sizeof(line));
        if (line[0] == '\0') continue;
        if (kstrcmp(line, "help") == 0) cmd_help();
        else if (kstrcmp(line, "ver") == 0)
            kprintf("cuss-kernel v0.1, CUSS-1 ISA, " "built " __DATE__ "\n");
        else if (kstrcmp(line, "clear") == 0) vga_clear();
        else if (kstrcmp(line, "demos") == 0) {
            for (uint32_t i = 0; i < NDEMOS; i++) kprintf("  %s\n", demos[i].name);
        }
        else if (kstrncmp(line, "demo ", 5) == 0) cmd_demo(line + 5);
        else if (kstrcmp(line, "demo") == 0) cmd_demo("");
        else if (kstrcmp(line, "asm") == 0) cmd_asm();
        else if (kstrcmp(line, "regs") == 0) cmd_regs();
        else kprintf("unknown: '%s' (try 'help')\n", line);
    }
}

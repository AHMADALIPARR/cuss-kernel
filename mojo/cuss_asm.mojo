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

# mojo/cuss_asm.mojo - CUSS-1 reference assembler (host tool).
#
# Same ISA.md contract as kernel/cuss/cuss_asm.c: two passes, 32-bit words
# printed one per line as 0x........ Intended as the readable reference;
# the kernel embeds the C version and runs it on bare metal.
#
# Requires the Modular Mojo toolchain (mojo 1.x):
#   mojo build cuss_asm.mojo -o cuss_asm
#   ./cuss_asm ../programs/fib.cuss

from std.sys import argv


def op_code(mn: String) -> Int:
    if mn == "nop": return 0x00
    if mn == "hlt": return 0x01
    if mn == "mov": return 0x02
    if mn == "movi": return 0x03
    if mn == "add": return 0x04
    if mn == "sub": return 0x05
    if mn == "mul": return 0x06
    if mn == "div": return 0x07
    if mn == "and": return 0x08
    if mn == "or": return 0x09
    if mn == "xor": return 0x0A
    if mn == "load": return 0x0B
    if mn == "stor": return 0x0C
    if mn == "jmp": return 0x0D
    if mn == "jz": return 0x0E
    if mn == "jnz": return 0x0F
    if mn == "out": return 0x10
    if mn == "in": return 0x11
    if mn == "call": return 0x12
    if mn == "ret": return 0x13
    return -1


def hex8(v: Int) -> String:
    var digits = String("0123456789abcdef")
    var s = String("0x")
    for i in range(8):
        var shift = 28 - i * 4
        s += String(digits[byte=(v >> shift) & 0xF])
    return s


def ch(s: String, i: Int) -> String:
    return String(s[byte=i])


def parse_u32(s: String) -> Int:
    """Decimal or 0x-hex; -1 on failure or > 0xFFFF. ASCII only."""
    var n = s.byte_length()
    var base = 10
    var start = 0
    if n >= 2 and ch(s, 0) == "0" and (ch(s, 1) == "x" or ch(s, 1) == "X"):
        base = 16
        start = 2
    var v = 0
    var count = 0
    var zero = ord("0")
    var nine = ord("9")
    var loa = ord("a")
    var hif = ord("f")
    var loA = ord("A")
    var hiF = ord("F")
    for i in range(start, n):
        var o = ord(ch(s, i))
        var d = -1
        if o >= zero and o <= nine:
            d = o - zero
        elif base == 16 and o >= loa and o <= hif:
            d = o - loa + 10
        elif base == 16 and o >= loA and o <= hiF:
            d = o - loA + 10
        else:
            return -1
        if d >= base:
            return -1
        v = v * base + d
        count += 1
    if count == 0 or v > 0xFFFF:
        return -1
    return v


def parse_reg(s: String) -> Int:
    var t = String(s.strip())
    if t.byte_length() < 2:
        return -1
    var c0 = ch(t, 0)
    if c0 != "r" and c0 != "R":
        return -1
    var v = parse_u32(String(t[byte=1:t.byte_length()]))
    if v < 0 or v > 15:
        return -1
    return v


def strip_comment(line: String) -> String:
    var n = line.byte_length()
    for i in range(n):
        if ch(line, i) == ";":
            return String(line[byte=0:i])
    return line


def tokenize(line: String) -> List[String]:
    var toks = List[String]()
    var cur = String("")
    var n = line.byte_length()
    for i in range(n):
        var c = ch(line, i)
        if c == " " or c == "\t" or c == ",":
            if cur.byte_length() > 0:
                toks.append(cur)
                cur = String("")
        else:
            cur += c
    if cur.byte_length() > 0:
        toks.append(cur)
    return toks^


def enc_r(op: Int, rd: Int, rs: Int, rt: Int) -> Int:
    return (op << 24) | ((rd & 15) << 20) | ((rs & 15) << 16) | ((rt & 15) << 12)


def enc_i(op: Int, rd: Int, imv: Int) -> Int:
    return (op << 24) | ((rd & 15) << 20) | (imv & 0xFFFF)


def enc_ri(op: Int, rd: Int, rs: Int, imv: Int) -> Int:
    return (op << 24) | ((rd & 15) << 20) | ((rs & 15) << 16) | (imv & 0xFFFF)


def enc_j(op: Int, addr: Int) -> Int:
    return (op << 24) | (addr & 0xFFFF)


def resolve_imm(tok: String, labels: Dict[String, Int]) raises -> Int:
    var imm = parse_u32(tok)
    if imm >= 0:
        return imm
    var key = tok.lower()
    if key in labels:
        return labels[key]
    return -1


def assemble(src: String) raises -> List[Int]:
    var labels = Dict[String, Int]()
    var words = List[Int]()

    # pass 1: collect labels, count words
    var addr = 0
    for _raw in src.splitlines():
        var line = String(strip_comment(String(_raw)).strip())
        if line.byte_length() == 0:
            continue
        if line.endswith(":"):
            var name = String(line[byte=0:line.byte_length() - 1]).strip().lower()
            if name.byte_length() == 0 or name in labels:
                print("error: bad or duplicate label: " + line)
                return List[Int]()
            labels[name] = addr
        else:
            addr += 1

    # pass 2: encode
    var lineno = 0
    for _raw in src.splitlines():
        lineno += 1
        var line = String(strip_comment(String(_raw)).strip())
        if line.byte_length() == 0 or line.endswith(":"):
            continue
        var toks = tokenize(line)
        if len(toks) == 0:
            continue
        var mn = toks[0].lower()
        var op = op_code(mn)
        if op < 0:
            print("error line " + String(lineno) + ": unknown mnemonic: " + toks[0])
            return List[Int]()

        var w = -1
        if mn == "nop" or mn == "hlt" or mn == "ret":
            if len(toks) != 1:
                print("error line " + String(lineno) + ": takes no operands")
                return List[Int]()
            w = op << 24
        elif mn == "mov":
            if len(toks) != 3:
                print("error line " + String(lineno) + ": mov rd, rs")
                return List[Int]()
            var r1 = parse_reg(toks[1])
            var r2 = parse_reg(toks[2])
            if r1 < 0 or r2 < 0:
                print("error line " + String(lineno) + ": bad register")
                return List[Int]()
            w = enc_r(op, r1, r2, 0)
        elif mn == "movi":
            if len(toks) != 3:
                print("error line " + String(lineno) + ": movi rd, imm")
                return List[Int]()
            var r1 = parse_reg(toks[1])
            var imm = parse_u32(toks[2])
            if r1 < 0 or imm < 0:
                print("error line " + String(lineno) + ": bad operands")
                return List[Int]()
            w = enc_i(op, r1, imm)
        elif mn == "add" or mn == "sub" or mn == "mul" or mn == "div" or mn == "and" or mn == "or" or mn == "xor":
            if len(toks) != 4:
                print("error line " + String(lineno) + ": " + mn + " rd, rs, rt")
                return List[Int]()
            var r1 = parse_reg(toks[1])
            var r2 = parse_reg(toks[2])
            var r3 = parse_reg(toks[3])
            if r1 < 0 or r2 < 0 or r3 < 0:
                print("error line " + String(lineno) + ": bad register")
                return List[Int]()
            w = enc_r(op, r1, r2, r3)
        elif mn == "load" or mn == "stor":
            if len(toks) != 4:
                print("error line " + String(lineno) + ": " + mn + " rd, rs, imm")
                return List[Int]()
            var r1 = parse_reg(toks[1])
            var r2 = parse_reg(toks[2])
            var imm = parse_u32(toks[3])
            if r1 < 0 or r2 < 0 or imm < 0:
                print("error line " + String(lineno) + ": bad operands")
                return List[Int]()
            w = enc_ri(op, r1, r2, imm)
        elif mn == "out" or mn == "in":
            if len(toks) != 2:
                print("error line " + String(lineno) + ": " + mn + " rd")
                return List[Int]()
            var r1 = parse_reg(toks[1])
            if r1 < 0:
                print("error line " + String(lineno) + ": bad register")
                return List[Int]()
            w = enc_r(op, r1, 0, 0)
        elif mn == "jmp" or mn == "call":
            if len(toks) != 2:
                print("error line " + String(lineno) + ": " + mn + " label")
                return List[Int]()
            var imm = resolve_imm(toks[1], labels)
            if imm < 0:
                print("error line " + String(lineno) + ": bad target: " + toks[1])
                return List[Int]()
            w = enc_j(op, imm)
        elif mn == "jz" or mn == "jnz":
            if len(toks) != 3:
                print("error line " + String(lineno) + ": " + mn + " rd, label")
                return List[Int]()
            var r1 = parse_reg(toks[1])
            var imm = resolve_imm(toks[2], labels)
            if r1 < 0 or imm < 0:
                print("error line " + String(lineno) + ": bad operands")
                return List[Int]()
            w = enc_i(op, r1, imm)
        else:
            print("error line " + String(lineno) + ": unhandled: " + mn)
            return List[Int]()

        words.append(w)

    return words^


def read_file(path: String) raises -> String:
    with open(path, "r") as f:
        return f.read()


def main() raises:
    var args = argv()
    if len(args) != 2:
        print("usage: cuss_asm <file.cuss>")
        return
    var words = assemble(read_file(args[1]))
    for i in range(len(words)):
        print(hex8(words[i]))

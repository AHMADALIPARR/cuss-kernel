#!/usr/bin/env python3
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

"""check_mb.py - verify the multiboot1 header and 32-bit ELF-ness of the kernel."""
import struct
import sys

MB_MAGIC = 0x1BADB002


def main():
    path = sys.argv[1]
    with open(path, "rb") as f:
        data = f.read()
    assert data[:4] == b"\x7fELF", "not an ELF"
    assert data[4] == 1, "not 32-bit ELF"
    assert data[5] == 1, "not little-endian"
    # scan the first 32 KiB for the multiboot header (must be 4-byte aligned)
    found = False
    for off in range(0, min(len(data), 32768), 4):
        magic, flags, chk = struct.unpack_from("<III", data, off)
        if magic == MB_MAGIC and ((magic + flags + chk) & 0xFFFFFFFF) == 0:
            found = True
            print(f"multiboot1 header at file offset {off}, flags=0x{flags:08x}")
            break
    assert found, "multiboot1 magic not found"
    print(f"{path}: OK (32-bit LE ELF, multiboot1)")


if __name__ == "__main__":
    main()

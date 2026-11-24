# SPDX-License-Identifier: GPL-3.0-or-later
"""Embed build-generated SPIR-V as aligned uint32_t arrays."""
import pathlib
import struct
import sys
def words(path):
    data = pathlib.Path(path).read_bytes()
    if len(data) % 4 or len(data) < 20:
        raise ValueError("invalid SPIR-V size")
    values = struct.unpack("<" + "I" * (len(data) // 4), data)
    if values[0] != 0x07230203:
        raise ValueError("invalid SPIR-V magic")
    return values
def emit(name, path):
    v = words(path)
    lines = [", ".join(f"0x{x:08x}u" for x in v[i:i+8]) for i in range(0,len(v),8)]
    return f"static const uint32_t {name}[] = {{\n" + ",\n".join(lines) + "\n};\n"
pathlib.Path(sys.argv[3]).write_text("#include <stdint.h>\n" + emit("world_vert",sys.argv[1]) + emit("world_frag",sys.argv[2]), encoding="ascii")

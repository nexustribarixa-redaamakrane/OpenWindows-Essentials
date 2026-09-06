#!/usr/bin/env python3
"""
Generate sucs_ucd_names.h and sucs_ucd_names.c from UnicodeData.txt.

Usage: python gen_ucd_names.py [path_to_UnicodeData.txt]
       If omitted, downloads from unicode.org.
"""

import sys, os, urllib.request, struct

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.dirname(SCRIPT_DIR)
INCLUDE_DIR = os.path.join(PROJECT_ROOT, "include", "superunicode")
SRC_DIR = os.path.join(PROJECT_ROOT, "src")

def fetch_udata(path=None):
    if path and os.path.isfile(path):
        return path
    url = "https://www.unicode.org/Public/UCD/latest/ucd/UnicodeData.txt"
    dest = os.path.join(os.environ.get("TEMP", "."), "UnicodeData.txt")
    print(f"Downloading {url} ...")
    urllib.request.urlretrieve(url, dest)
    return dest

def parse_udata(path):
    entries = []
    with open(path, "r", encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            parts = line.split(";")
            if len(parts) < 2:
                continue
            cp = int(parts[0], 16)
            name = parts[1].strip()
            if name.startswith("<") and name.endswith(">"):
                continue
            if name == "<control>" or not name:
                continue
            entries.append((cp, name))
    entries.sort(key=lambda x: x[0])
    return entries

def build_pool_and_index(entries):
    pool = bytearray()
    index = []
    for cp, name in entries:
        offset = len(pool)
        name_bytes = name.encode("ascii")
        pool.extend(name_bytes)
        index.append((cp, offset, len(name_bytes)))
    return bytes(pool), index

def write_header(count, out_path):
    with open(out_path, "w", encoding="utf-8") as f:
        f.write("#ifndef SUPERUNICODE_SUCS_UCD_NAMES_H\n")
        f.write("#define SUPERUNICODE_SUCS_UCD_NAMES_H\n\n")
        f.write('#include "sucs_types.h"\n')
        f.write('#include "sucs_compat.h"\n\n')
        f.write("#ifdef __cplusplus\n")
        f.write('extern "C" {\n')
        f.write("#endif\n\n")
        f.write("/* ========================================================================\n")
        f.write(" * Unicode Character Database Name Lookup (Unicode 17.0)\n")
        f.write(" *\n")
        f.write(" * Provides codepoint -> character name resolution for all named\n")
        f.write(" * codepoints in the Unicode Compatibility Range (0x000000 - 0x10FFFF).\n")
        f.write(" * Data is stored in a packed string pool with a sorted index table\n")
        f.write(" * for O(log n) binary search lookup.\n")
        f.write(" * ======================================================================== */\n\n")
        f.write(f"/* Total named codepoints in database */\n")
        f.write(f"#define SUCS_UCD_NAME_COUNT {count}\n\n")
        f.write("/* Index entry: codepoint -> name pool offset */\n")
        f.write("typedef struct {\n")
        f.write("    uint32_t cp;           /* Unicode codepoint */\n")
        f.write("    uint32_t name_offset;  /* Byte offset into sucs_ucd_name_pool[] */\n")
        f.write("    uint16_t name_length;  /* Length of name string in bytes */\n")
        f.write("} sucs_ucd_name_entry_t;\n\n")
        f.write("/* Sorted index table (binary-searchable by codepoint) */\n")
        f.write("extern const sucs_ucd_name_entry_t sucs_ucd_name_index[SUCS_UCD_NAME_COUNT];\n\n")
        f.write("/* Packed string pool containing all character names */\n")
        f.write("extern const char sucs_ucd_name_pool[];\n\n")
        f.write("/* ========================================================================\n")
        f.write(" * Lookup API\n")
        f.write(" * ======================================================================== */\n\n")
        f.write("/**\n")
        f.write(" * Returns the Unicode character name for a given codepoint.\n")
        f.write(" * Returns NULL if the codepoint has no name in the UCD.\n")
        f.write(" * Uses O(log n) binary search on the sorted index table.\n")
        f.write(" */\n")
        f.write("const char* sucs_ucd_get_name(sucs_char_t cp);\n\n")
        f.write("/**\n")
        f.write(" * Returns the length of the Unicode character name for a given codepoint.\n")
        f.write(" * Returns 0 if the codepoint has no name in the UCD.\n")
        f.write(" */\n")
        f.write("uint16_t sucs_ucd_name_length(sucs_char_t cp);\n\n")
        f.write("/**\n")
        f.write(" * Copies the Unicode character name for a given codepoint into out_buf.\n")
        f.write(" * Returns the number of bytes copied, or 0 if not found or buffer too small.\n")
        f.write(" */\n")
        f.write("uint16_t sucs_ucd_get_name_copy(sucs_char_t cp, char* out_buf, uint16_t buf_size);\n\n")
        f.write("#ifdef __cplusplus\n")
        f.write("}\n")
        f.write("#endif\n\n")
        f.write("#endif /* SUPERUNICODE_SUCS_UCD_NAMES_H */\n")

def write_source(pool, index, out_path):
    with open(out_path, "w", encoding="utf-8") as f:
        f.write('#include "superunicode/sucs_ucd_names.h"\n')
        f.write("#include <stdint.h>\n")
        f.write("#include <stdbool.h>\n\n")
        f.write("/* ========================================================================\n")
        f.write(" * Auto-generated Unicode 17.0 Character Name Database\n")
        f.write(" * Source: https://www.unicode.org/Public/UCD/latest/ucd/UnicodeData.txt\n")
        f.write(f" * Entries: {len(index)} named codepoints\n")
        f.write(f" * Pool size: {len(pool)} bytes\n")
        f.write(" * ======================================================================== */\n\n")

        # Write string pool
        f.write("const char sucs_ucd_name_pool[] = {\n")
        for i in range(0, len(pool), 16):
            chunk = pool[i:i+16]
            hex_vals = ", ".join(f"0x{b:02x}" for b in chunk)
            f.write(f"    {hex_vals},\n")
        f.write("};\n\n")

        # Write index table
        f.write("const sucs_ucd_name_entry_t sucs_ucd_name_index[SUCS_UCD_NAME_COUNT] = {\n")
        for cp, offset, length in index:
            f.write(f"    {{ 0x{cp:08x}u, {offset:7d}u, {length:3d} }},\n")
        f.write("};\n\n")

        # Write lookup functions
        f.write("static int sucs_ucd_name_bsearch(sucs_char_t cp) {\n")
        f.write("    int lo = 0;\n")
        f.write("    int hi = SUCS_UCD_NAME_COUNT - 1;\n")
        f.write("    while (lo <= hi) {\n")
        f.write("        int mid = lo + (hi - lo) / 2;\n")
        f.write("        uint32_t mid_cp = sucs_ucd_name_index[mid].cp;\n")
        f.write("        if (cp < mid_cp) {\n")
        f.write("            hi = mid - 1;\n")
        f.write("        } else if (cp > mid_cp) {\n")
        f.write("            lo = mid + 1;\n")
        f.write("        } else {\n")
        f.write("            return mid;\n")
        f.write("        }\n")
        f.write("    }\n")
        f.write("    return -1;\n")
        f.write("}\n\n")

        f.write("const char* sucs_ucd_get_name(sucs_char_t cp) {\n")
        f.write("    int idx = sucs_ucd_name_bsearch(cp);\n")
        f.write("    if (idx < 0) return (const char*)0;\n")
        f.write("    return &sucs_ucd_name_pool[sucs_ucd_name_index[idx].name_offset];\n")
        f.write("}\n\n")

        f.write("uint16_t sucs_ucd_name_length(sucs_char_t cp) {\n")
        f.write("    int idx = sucs_ucd_name_bsearch(cp);\n")
        f.write("    if (idx < 0) return 0;\n")
        f.write("    return sucs_ucd_name_index[idx].name_length;\n")
        f.write("}\n\n")

        f.write("uint16_t sucs_ucd_get_name_copy(sucs_char_t cp, char* out_buf, uint16_t buf_size) {\n")
        f.write("    int idx = sucs_ucd_name_bsearch(cp);\n")
        f.write("    if (idx < 0) return 0;\n")
        f.write("    uint16_t len = sucs_ucd_name_index[idx].name_length;\n")
        f.write("    if (len >= buf_size) return 0;\n")
        f.write("    const char* src = &sucs_ucd_name_pool[sucs_ucd_name_index[idx].name_offset];\n")
        f.write("    for (uint16_t i = 0; i < len; i++) {\n")
        f.write("        out_buf[i] = src[i];\n")
        f.write("    }\n")
        f.write("    out_buf[len] = '\\0';\n")
        f.write("    return len;\n")
        f.write("}\n")

def main():
    udata_path = sys.argv[1] if len(sys.argv) > 1 else None
    path = fetch_udata(udata_path)
    print(f"Parsing {path} ...")
    entries = parse_udata(path)
    print(f"Found {len(entries)} named codepoints")
    pool, index = build_pool_and_index(entries)
    print(f"String pool: {len(pool)} bytes, Index: {len(index)} entries")

    header_path = os.path.join(INCLUDE_DIR, "sucs_ucd_names.h")
    source_path = os.path.join(SRC_DIR, "sucs_ucd_names.c")

    print(f"Writing {header_path} ...")
    write_header(len(index), header_path)
    print(f"Writing {source_path} ...")
    write_source(pool, index, source_path)
    print("Done.")

if __name__ == "__main__":
    main()

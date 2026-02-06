import argparse as ap
import sys
import parser
import os
import pycparser as pc
import re
import subprocess as sb
import shutil

TEMP_WD = "__temp__crefl"
BIG_H = "__reflected_types.h"

def parse_header(dot_h_path: str, enum_hints, struct_hints, union_hints):
    print(f"Tried to parse: {dot_h_path}")
    ast = pc.parse_file(dot_h_path)
    v = parser.ReflVisitor(
        enum_hints=enum_hints, 
        struct_hints=struct_hints, 
        union_hints=union_hints)
    v.visit(ast)


def cleanup():
    os.chdir("..")
    shutil.rmtree(TEMP_WD)

def extract_types(dot_h_path: str) -> str:
    with open(dot_h_path, 'r') as f:
        content = f.read()

    # Find all REFLECT blocks using your brace-counting logic
    # (Or the non-greedy regex if you prefer)
    blocks = []
    # Simplified regex for finding the starts
    starts = re.finditer(r'REFLECT\(.*?\)\s*{', content)
    
    for start in starts:
        st = start.end() # at the '{'
        idx = st 
        brace_level = 1

        while idx < len(content):
            char = content[idx]
            if   char == '{' : brace_level += 1
            elif char == '}' : brace_level -= 1
            if brace_level == 0 and char == ';': break
            idx += 1
            
        block = content[start.start():idx + 1]
        blocks.append(block)
        pass

    clean_content = "\n".join(blocks)
    
    return clean_content

def expand_headers(headers: list[str]) -> str:
    try:
        shutil.rmtree(TEMP_WD)
    except FileNotFoundError: ...
    os.mkdir(TEMP_WD)
    os.chdir(TEMP_WD)

    combined = "#include <stdint.h>\n#include <stddef.h>\n#include <stdbool.h>\n#include <min_refl_types.h>\n"



    for header in headers:
        combined += extract_types(f"../{header}") + "\n"

    with open(BIG_H, "w") as f:
        f.write(combined)

    try:
        sb.run(
            ["gcc","-E", "-P",
                BIG_H,
                "-nostdinc",
                "-o", f"exp_{BIG_H}", 
                f"-I../inyect_headers"
                ], check=True)
    except sb.CalledProcessError:
        # cleanup()
        re = RuntimeError("C Compilation failed")
        re.add_note("One of the input headers did not exist or couldn't be compiled")
        raise re
        
    os.chdir("..")
    return f"exp_{BIG_H}"


def parse_args() -> tuple[str, list[str], str]:
    parser = ap.ArgumentParser(description="Reflective types generator", add_help=True)
    parser.add_argument('-o', '--output', help="Output .crefl file", type=str, default="generated_types.crefl")
    parser.add_argument('-i', '--input', help="Input header (.h) files with types to reflect (order is important)", default=[], nargs='+')
    args = parser.parse_args()
    return args.output, args.input



if __name__ == "__main__":
    output, header_files = parse_args()
    out_h = expand_headers(header_files)
    if len(header_files) <= 0:
        print("no headers received as input", file=sys.stderr)
        exit(1)
    for doth in header_files:
        eh = parser.extract_enum_hints(doth)
        sth, unh = parser.extract_struct_union_hints(doth)
    os.chdir(TEMP_WD)
    parse_header(out_h, eh, sth, unh)
    cleanup()

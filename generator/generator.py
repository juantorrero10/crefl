import parser
import os
import pycparser as pc
import re
import subprocess as sb
import hashlib
import shutil

TEMP_WD = "__temp__crefl"
BIG_H = "__reflected_types.h"
file_path = os.path.abspath(os.path.dirname(__file__))


class CReflEngine:
    def __init__(self, headers: list[str], checksum):
        self.headers = headers
        self.hints_enum = []
        self.hints_struct = {}
        self.hints_union = {}
        self.checksum = checksum
        
    def generate(self, output_file: str):
        for header in self.headers:
            enums = self._extract_enum_hints(header)
            structs, unions = self._extract_struct_union_hints(header)

            self.hints_enum.extend(enums)
            self.hints_struct.update(structs)
            self.hints_union.update(unions)

        new_cs, out_h = self._expand_headers()
        if out_h is None: return None, None
        return new_cs, self._parse_header(f"./{out_h}")

    def _cleanup(self):
        os.chdir("..")
        shutil.rmtree(TEMP_WD)
        
    def _parse_header(self, dot_h_path: str):
        print(f"Tried to parse: {dot_h_path}")
        ast = pc.parse_file(dot_h_path)
        v = parser.ReflVisitor( 
            dot_h_path,
            enum_hints=self.hints_enum, 
            struct_hints=self.hints_struct, 
            union_hints=self.hints_union
        )
        v.visit(ast)
        self._cleanup()
        return v.reflected

    def _extract_types(self, dot_h_path: str) -> str:
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

    def _expand_headers(self) -> tuple[str, str | None]:
        try:
            shutil.rmtree(TEMP_WD)
        except FileNotFoundError: ...
        os.mkdir(TEMP_WD)
        os.chdir(TEMP_WD)

        combined = "#include <stdint.h>\n#include <stddef.h>\n#include <stdbool.h>\n#include <min_refl_types.h>\n"



        for header in self.headers:
            combined += self._extract_types(f"../{header}") + "\n"

        with open(BIG_H, "w") as f:
            f.write(combined)

        new_checksum = hashlib.md5(combined.encode("utf-8")).digest()

        if self.checksum == new_checksum:
            # Checksum matches, nothing to be done
            return None, None

        try:
            sb.run(
                ["gcc","-E", "-P",
                    BIG_H,
                    "-nostdinc",
                    "-o", f"exp_{BIG_H}", 
                    f"-I{file_path}/inyect_headers"
                    ], check=True)
        except sb.CalledProcessError:
            self._cleanup()
            re = RuntimeError("C Compilation failed")
            re.add_note("One of the input headers did not exist or couldn't be compiled")
            raise re
            
        return new_checksum, f"exp_{BIG_H}"
    

    def _extract_enum_hints(seld, dot_h_path: str) -> list[str]:
        with open(dot_h_path) as f:
            content = f.read()

        ret = []

        reflects = re.finditer(r'REFLECT\(enum\)\s*{(?P<body>.+?)}\s*(?P<typename>\w+);', content, re.DOTALL)
        for enum in reflects:
            ret.append(enum.group("typename"))

        return ret



    def _extract_struct_union_hints(self, dot_h_path: str) -> tuple[ dict[dict[str, str], str], dict[dict[str, str], str] ]:

        struct_hints = {}
        union_hints = {}
        
        with open(dot_h_path) as f:
            content = f.read()

        block_starts = re.finditer(r'REFLECT\((?P<kind>[^)]+)\)\s*{', content)

        for start in block_starts:
            struct_or_union = True
            match start.group("kind"):
                case "enum":
                    continue
                case "struct":
                    struct_or_union = False
                case _:
                    struct_or_union = True
    

            st = start.end() # at the '{'
            idx = st 
            brace_level = 1

            while(idx < len(content) and brace_level > 0):
                char = content[idx]
                if   char == '{' : brace_level += 1
                elif char == '}' : brace_level -= 1
                idx += 1
            
            body = content[st : idx - 1]
            typename = re.search(r'\s*(?P<name>\w+);', content[idx:])
            if not typename: continue
            typename = typename.group("name")
            if struct_or_union:
                union_hints[typename] = {}
            else:
                struct_hints[typename] = {}

            patterns = [
                r'DYN_ARRAY\((?P<type>[^)]+)\)\s+(?P<name>\w+)',
                r'OPTION\((?P<type>[^)]+)\)\s+(?P<name>\w+)'
            ]

            for pattern in patterns:
                for match in re.finditer(pattern, body):
                    if struct_or_union:
                        union_hints[typename][match.group("name")] = match.group("type")
                    else:
                        struct_hints[typename][match.group("name")] = match.group("type")


        return struct_hints, union_hints



    

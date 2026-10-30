import parser
import os
import pycparser as pc
import re
import prober
import subprocess as sb
import hashlib
import shutil
import struct
from pathlib import Path

TEMP_WD = "__temp__crefl"
BIG_H = "__reflected_types.h"
file_path = os.path.abspath(os.path.dirname(__file__))


class CReflEngine:
    def __init__(self, headers: list[str], checksum):
        self.headers = headers
        self.hints_enum = []
        self.hints_struct = {}
        self.hints_union = {}
        self.option_hints = {}
        self.checksum = checksum
        
    def generate(self, output_file: str):
        for header in self.headers:
            enums = self._extract_enum_hints(header)
            structs, unions = self._extract_struct_union_hints(header)
            options = self._extract_option_hints(header)

            self.hints_enum.extend(enums)
            self.hints_struct.update(structs)
            self.hints_union.update(unions)
            self.option_hints.update(options)

        new_cs, out_h = self._expand_headers()
        if out_h is None: return None, None
        return new_cs, self._parse_header(f"./{out_h}")

    def _cleanup(self):
        os.chdir("..")
        shutil.rmtree(TEMP_WD)
        
    def _parse_header(self, dot_h_path: str):
        try:
            ast = pc.parse_file(dot_h_path)
            with open(dot_h_path) as f:
                expanded_header = f.read()
            available_option_types = set(re.findall(
                r'\b(__OPTION_(?:DA_|FA_)?\w+_DECL__)\b',
                expanded_header,
            ))
            option_members = {
                option_type: (
                    "data" if hint["kind"] == "option" else "arr"
                )
                for option_type, hint in self.option_hints.items()
                if option_type in available_option_types
            }
            option_layouts = prober.get_option_layouts(dot_h_path, option_members)
            v = parser.ReflVisitor(
                dot_h_path,
                enum_hints=self.hints_enum,
                struct_hints=self.hints_struct,
                union_hints=self.hints_union,
                option_hints=self.option_hints,
                option_layouts=option_layouts,
            )
            v.visit(ast)
            return v.reflected
        finally:
            self._cleanup()

    def _extract_types(self, dot_h_path: str) -> str:
        with open(dot_h_path, 'r') as f:
            content = f.read()

        blocks = []
        starts = re.finditer(r'REFLECT\([^)]*\)\s*{', content)

        for start in starts:
            st = start.end()
            idx = st
            brace_level = 1

            while idx < len(content):
                char = content[idx]
                if char == '{':
                    brace_level += 1
                elif char == '}':
                    brace_level -= 1
                    if brace_level == 0:
                        semicolon = content.find(';', idx)
                        if semicolon != -1:
                            idx = semicolon
                        break
                idx += 1

            block = content[start.start():idx + 1]
            reflected_name = re.search(r'}\s*(?P<name>\w+)\s*;', block)
            if not reflected_name or reflected_name.group("name") != parser.STRING_TYPE:
                blocks.append((start.start(), block))

        option_declaration_pattern = re.compile(
            r'\bDECL_OPTION(?P<kind>_DYN_ARRAY|_FIXED_ARRAY)?\s*'
            r'\((?P<args>[^;]*?)\)\s*;'
        )
        injected_options = self._extract_option_hints(
            os.path.join(file_path, "inyect_headers/min_refl_types.h")
        )
        for match in option_declaration_pattern.finditer(content):
            args = [arg.strip() for arg in match.group("args").split(",")]
            alias, _ = self._option_hint(match.group("kind"), args, match.group())
            if alias not in injected_options:
                blocks.append((match.start(), match.group()))

        clean_content = "\n".join(block for _, block in sorted(blocks))
        
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

        checksum_input = [combined]
        injected_headers = Path(file_path, "inyect_headers")
        for header in sorted(injected_headers.glob("*.h")):
            checksum_input.extend((header.name, header.read_text(encoding="utf-8")))
        new_checksum = hashlib.md5("\0".join(checksum_input).encode("utf-8")).digest()

        if self.checksum == new_checksum:
            # Checksum matches, nothing to be done
            self._cleanup()
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

    def _extract_option_hints(self, dot_h_path: str) -> dict[str, dict[str, str]]:
        with open(dot_h_path) as f:
            content = f.read()

        hints = {}
        declaration_pattern = re.compile(
            r'\bDECL_OPTION(?P<kind>_DYN_ARRAY|_FIXED_ARRAY)?\s*'
            r'\((?P<args>[^()]*)\)\s*;'
        )
        usage_pattern = re.compile(
            r'\bOPTION(?P<kind>_DYN_ARRAY|_FIXED_ARRAY)?\s*'
            r'\((?P<args>[^()]*)\)\s+(?P<name>\w+)\s*;'
        )

        for pattern in (declaration_pattern, usage_pattern):
            for match in pattern.finditer(content):
                args = [arg.strip() for arg in match.group("args").split(",")]
                alias, hint = self._option_hint(
                    match.group("kind"),
                    args,
                    match.group(),
                )
                hints[alias] = hint

        return hints

    @staticmethod
    def _option_hint(
        kind: str | None,
        args: list[str],
        declaration: str,
    ) -> tuple[str, dict[str, str]]:
        if kind is None and len(args) == 1:
            return f"__OPTION_{args[0]}_DECL__", {"kind": "option", "type": args[0]}
        if kind == "_DYN_ARRAY" and len(args) == 1:
            return f"__OPTION_DA_{args[0]}_DECL__", {"kind": "dyn_array", "type": args[0]}
        if kind == "_FIXED_ARRAY" and len(args) == 2:
            return (
                f"__OPTION_FA_{args[0]}_{args[1]}_DECL__",
                {"kind": "fixed_array", "type": args[0], "size": args[1]},
            )
        raise ValueError(f"Invalid option declaration: {declaration}")


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



    

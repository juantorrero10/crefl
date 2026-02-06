import pycparser.c_ast as c_ast
import re

PRIMITIVE_MAP = {
    # Integers
    'int8_t': 'P_I8',
    'uint8_t': 'P_U8',
    'int16_t': 'P_I16',
    'uint16_t': 'P_U16',
    'int32_t': 'P_I32',
    'uint32_t': 'P_U32',
    'int64_t': 'P_I64',
    'uint64_t': 'P_U64',
    'float': 'P_F32',
    'double': 'P_F64',
    'bool': 'P_BOOL', '_Bool': 'P_BOOL'
}

PRIMITIVE_VALUES = {
    'P_U8': 0, 'P_I8': 1, 'P_U16': 2, 'P_I16': 3,
    'P_U32': 4, 'P_I32': 5, 'P_U64': 6, 'P_I64': 7,
    'P_F32': 8, 'P_F64': 9, 'P_BOOL': 10
}

REFL_CUSTOM = {
    'r_option_t': 'K_OPTION',
    'r_array_t': 'K_DYN_ARRAY'
}

def extract_enum_hints(dot_h_path: str) -> list[str]:
    with open(dot_h_path) as f:
        content = f.read()

    ret = []

    reflects = re.finditer(r'REFLECT\(enum\)\s*{(?P<body>.+?)}\s*(?P<typename>\w+);', content, re.DOTALL)
    for enum in reflects:
        ret.append(enum.group("typename"))

    return ret



def extract_struct_union_hints(dot_h_path: str) -> tuple[ dict[dict[str, str], str], dict[dict[str, str], str] ]:

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


def print_extracted(header):
    enum_hints = extract_enum_hints(header)
    st_h, un_h = extract_struct_union_hints(header)
    print("Enums: ")
    for e in enum_hints:
        print(f"    Refl enum: {e}")
    print("Unions: ")
    for name, u in un_h.items():
        print(f"    Refl union: {name}")
        for f, s in u.items():
            print(f"        Field: {f} -> Subtype: {s}")
    print("Structs: ")
    for name, st in st_h.items():
        print(f"    Refl struct: {name}")
        for f, s in st.items():
            print(f"        Field: {f} -> Subtype: {s}")


class ReflVisitor(c_ast.NodeVisitor):
    def __init__(self, enum_hints={}, struct_hints={}, union_hints={}):
        self.henums = enum_hints
        self.hstructs = struct_hints
        self.hunions = union_hints

    def visit_Typedecl(self, node):
        typename = node.name
        # Check keys explicitly
        if (typename in self.henums or 
            typename in self.hstructs or 
            typename in self.hunions):
            
            print(f"\n[FOUND REFLECTED TYPE]: {typename}")
            
            # Drill down to find the struct/union/enum members
            # Typedef -> TypeDecl -> Struct/Union/Enum
            target = node.type.type
            
            if hasattr(target, 'decls') and target.decls:
                for member in target.decls:
                    print(f"  Field: {member.name}")
            elif isinstance(target, c_ast.Enum):
                for item in target.values.enumerators:
                    print(f"  Enum Constant: {item.name}")

    

if __name__ == '__main__':
    print_extracted("../src/type.h")
        




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

    def visit_TypeDecl(self, node: c_ast.TypeDecl):
        typename = node.declname  # <-- use declname, not name

        if typename in self.henums or typename in self.hstructs or typename in self.hunions:
            print(f"\n[FOUND REFLECTED TYPE]: {typename}")

            target = node.type  # type can be Struct, Union, Enum, IdentifierType

            if isinstance(target, c_ast.Struct) or isinstance(target, c_ast.Union):
                if target.decls:  # may be None for empty structs/unions
                    for member in target.decls:
                        print(f"  Field: {member.name} ({member.type.__class__.__name__})")

            elif isinstance(target, c_ast.Enum):
                for item in target.values.enumerators:
                    print(f"  Enum Constant: {item.name}")

    

if __name__ == '__main__':
    print_extracted("../src/type.h")
        




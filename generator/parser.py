import pycparser.c_ast as c_ast
import re
import prober
from enum import Enum

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
    'size_t': 'P_U64',
    'ssize_t': 'P_I64',
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

class SubtypeKind(Enum):
    PRIM_TYPE:  int = 0,
    SUBFIELD:   int = 1,
    ENUM:       int = 2,
    STRUCT:     int = 3,
    UNION:      int = 4,

"""
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
"""

def pretty(d, indent=0):
   for key, value in d.items():
      print('\t' * indent + str(key))
      if isinstance(value, dict):
         pretty(value, indent+1)
      else:
         print('\t' * (indent+1) + str(value))


class ReflVisitor(c_ast.NodeVisitor):
    def __init__(self, dot_h_path: str, enum_hints={}, struct_hints={}, union_hints={}):
        self.henums = enum_hints
        self.hstructs = struct_hints
        self.hunions = union_hints
        self.doth = dot_h_path
        self.reflected = {
            "Enums": [],
            "Structs": [],
            "Unions": []
        }

    

    def visit_TypeDecl(self, node: c_ast.TypeDecl):
        typename = node.declname  # <-- use declname, not name
        

        if typename in self.henums or typename in self.hstructs or typename in self.hunions:
            # print(f"\n[FOUND REFLECTED TYPE]: {typename}")

            target = node.type  # type can be Struct, Union, Enum, IdentifierType

            if isinstance(target, c_ast.Struct):
                if target.decls:  # may be None for empty structs/unions
                    self.reflected["Structs"].append(self._process_struct(typename, target))

            elif isinstance(target, c_ast.Enum):
                # self.reflected["Enums"].append(self._process_enum(typename, target))
                # for item in target.values.enumerators:
                #    print(f"  Enum Constant: {item.name}")
                ...

    """
    typedef struct { 
    const char* typename;
    const char** names;
    size_t* indexes;
    size_t n_elements;
    } REnum;
    """
    def _process_enum(self, typename: str, node: c_ast.Enum) -> dict[str, any]:

        enum = {
            "typename" : typename,
            "names" : [],
            "indexes" : [],
            "n_elements": 0
        }

        current_value = -1

        for item in node.values.enumerators:
            enum["n_elements"] += 1
            enum["names"].append(item.name)

            if item.value is not None:
                current_value = self._const_to_int(item.value)
            else:
                current_value += 1

            enum["indexes"].append(current_value)

        return 
    
    """
    typedef struct _CREFL_FIELD_{
        const char* name;
        size_t offset;
        size_t size;           
        RKind kind;
        union {
            KPrimitive prim;
            RField* subfield;
            REnum* e;
            RStruct* s;
            RUnion* u;
        } subtype;
    } RField;
    """
    def _process_field(self, name: str, offset: int, decl_type, parent_struct: str) -> dict[str: any]:
        field = {
            "name": name,
            "offset": offset,
            "size": 1,
            "kind": None,
            "subtype": {}
        }

        base_type = self._get_base_type(decl_type)

        # PRIMITIVES
        if base_type in PRIMITIVE_MAP:
                prim = PRIMITIVE_MAP[base_type]
                field["kind"] = "K_PRIMITIVE"
                field["subtype"] = {
                    "kind": SubtypeKind.PRIM_TYPE,
                    "data": prim
                }
                return field
        
        #Option<T> / DynArray<T>
        if base_type in REFL_CUSTOM:
            field["kind"] = REFL_CUSTOM[base_type]

            # Resolve contained type from hints
            try:
                contained_type = self.hstructs[parent_struct][name]
            except KeyError:
                raise RuntimeError(
                    f"Missing generic hint for {parent_struct}.{name}"
                )

            # Recurse into contained type
            subfield = self._process_field(
                name=f"{name}_value",
                offset=0,
                decl_type=c_ast.IdentifierType([contained_type]),
                parent_struct=parent_struct
            )

            field["subtype"] = {
                "kind": SubtypeKind.SUBFIELD,
                "data": subfield
            }
            return
        

        # Structs, they need the REFLECT(...) too
        if isinstance(decl_type, c_ast.Struct):
            field["kind"] = "K_STRUCT"
            if decl_type.name not in self.hstructs:
                raise RecursionError(
                    f"Nested struct: f{decl_type} in type: '{parent_struct}' was not marked with 'REFLECT(...)'"
                )
            field["subtype"] = {
                "kind": SubtypeKind.STRUCT,
                "data": decl_type.name
            }
            return field
        
        # Unions, also reflected
        if isinstance(decl_type, c_ast.Struct):
            field["kind"] = "K_STRUCT"
            if decl_type.name not in self.hunions:
                raise RecursionError(
                    f"Nested union: f{decl_type} in type: '{parent_struct}' was not marked with 'REFLECT(...)'"
                )
            field["subtype"] = {
                "kind": SubtypeKind.UNION,
                "data": decl_type.name
            }
            return field
        # Enums, also reflected
        if isinstance(decl_type, c_ast.Struct):
            field["kind"] = "K_STRUCT"
            if decl_type.name not in self.henums:
                raise RecursionError(
                    f"Nested enum: f{decl_type} in type: '{parent_struct}' was not marked with 'REFLECT(...)'"
                )
            field["subtype"] = {
                "kind": SubtypeKind.ENUM,
                "data": decl_type.name
            }
            return field

        # Raw pointers and strings
        if isinstance(decl_type, c_ast.PtrDecl):
            if base_type.endswith('char'):
                field["kind"] = "K_STRING"
            else:
                field["kind"] = "K_POINTER"

                pointee_field = self._process_field(
                    name=f"{name[:-1]}",
                    offset=0,
                    decl_type=decl_type.type,
                    parent_struct=parent_struct
                )

                field["subtype"] = pointee_field
                return field
            
        # Fixed-Sixed Arrays
        if isinstance(decl_type, c_ast.ArrayDecl):
            field["kind"] = "K_FIXED_ARRAY"
            field["size"] = decl_type.dim
            field["subtype"] = self._process_field(
                    name=f"{name[:-1]}",
                    offset=0,
                    decl_type=decl_type.type,
                    parent_struct=parent_struct
                )
            return field



            
    """
    typedef struct {
    const char* typename;
    RField* fields;
    size_t n_elements;
    } RStruct, RUnion;
    """
    def _process_struct(self, typename: str, node: c_ast.Struct) -> dict[str, any]:

        print(f"Processing type: {typename}")
        struct = {
            "typename": typename,
            "fields": [],
            "n_elements": 0
        }

        # extract offsets first
        field_names = []
        for field in node.decls:
            field_names.append(field.name)

        offsets = prober.get_offsets_of_type(self.doth, typename, field_names)
        print(offsets)

        for idx, field in enumerate(node.decls):
            field_res = self._process_field(
                field.name, offsets[idx], field.type.type, typename)

            struct["fields"].append(field_res)
            struct["n_elements"] += 1

            
        self.reflected["Structs"].append(struct)

        print(struct)
    
    def _const_to_int(self, c: c_ast.Constant) -> int:
        assert c.type == "int"

        text = c.value.lower()

        # Hexadecimal
        if text.startswith("0x"):
            return int(text, 16)

        # Octal (C-style: leading 0)
        if text.startswith("0") and text != "0":
            return int(text, 8)

        # Decimal
        return int(text, 10)
    
    def _get_base_type(self, decl) -> str | None:
        if (isinstance(decl, c_ast.TypeDecl) 
            or isinstance(decl, c_ast.PtrDecl)
            or isinstance(decl, c_ast.ArrayDecl)):
            return decl.type
        elif isinstance(decl, c_ast.IdentifierType):
            return " ".join(decl.names)

        return None


        




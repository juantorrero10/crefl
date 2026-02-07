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
    'bool': 'P_BOOL', '_Bool': 'P_BOOL',
    'void': 'P_U0'
}

KIND_VALUES = {
    'K_PRIMITIVE':0,
    'K_STRING': 1,
    'K_POINTER': 2,
    'K_STRUCT': 3,
    'K_UNION': 4,
    'K_ENUM': 5,
    'K_DYN_ARRAY': 6,
    'K_FIXED_ARRAY': 7,
    'K_OPTION': 8,
}

PRIMITIVE_VALUES = {
    'P_U8': 0, 'P_I8': 1, 'P_U16': 2, 'P_I16': 3,
    'P_U32': 4, 'P_I32': 5, 'P_U64': 6, 'P_I64': 7,
    'P_F32': 8, 'P_F64': 9, 'P_BOOL': 10, 'P_U0': 11
}

REFL_CUSTOM = {
    'r_option_t': 'K_OPTION',
    'r_array_t': 'K_DYN_ARRAY'
}

class SubtypeKind(Enum):
    NONE:       int = -1
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
        # Result of the operation
        self.reflected = {
            "Enums": [],
            "Structs": [],
            "Unions": []
        }

        # Temp lookup dicts
        self.r_enums = {}
        self.r_structs = {}
        self.r_unions = {}

    

    def visit_TypeDecl(self, node: c_ast.TypeDecl):
        typename = node.declname  # <-- use declname, not name
        

        if typename in self.henums or typename in self.hstructs or typename in self.hunions:
            target = node.type  # type can be Struct, Union, Enum, IdentifierType

            if isinstance(target, c_ast.Struct):
                if target.decls:  # may be None for empty structs/unions
                    self.reflected["Structs"].append(self._process_struct(typename, target))
            if isinstance(target, c_ast.Union):
                if target.decls:  # may be None for empty structs/unions
                    self.reflected["Unions"].append(self._process_struct(typename, target, union=True))


            elif isinstance(target, c_ast.Enum):
                self.reflected["Enums"].append(self._process_enum(typename, target))
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
        self.r_enums[typename] = enum
        return enum
    
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
    def _process_field(self, name: str, offset: int, decl_type, parent_struct: str, is_struct: bool) -> dict[str: any]:
        field = {
            "name": name,
            "offset": offset,
            "size": 1,
            "kind": None,
            "subtype": {}
        }

        pull_from = self.hstructs if is_struct else self.hunions

        base_type = self._get_base_type(decl_type)

        # Raw pointers and strings
        if isinstance(decl_type, c_ast.PtrDecl):
            subtype = {}
            if (base_type == 'char') and not isinstance(decl_type.type, c_ast.PtrDecl):
                field["kind"] = "K_STRING"
            else:
                field["kind"] = "K_POINTER"

                pointee_field = self._process_field(
                    name=f"{name[:-1]}",
                    offset=0,
                    decl_type=decl_type.type,
                    parent_struct=parent_struct, 
                    is_struct=is_struct
                )
                subtype = {
                    "kind": SubtypeKind.SUBFIELD,
                    "data": pointee_field
                }

            field["subtype"] = subtype
            return field

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
            try:
                contained = pull_from[parent_struct][name]
            except KeyError:
                raise RuntimeError(
                f"Couldn't find macro hint for {parent_struct}::{name}"
            )

            sub = self._process_field(
                name, 0,
                self._str_type_to_decl(contained),
                parent_struct,
                is_struct
            )
            field["subtype"] = {
                "kind": SubtypeKind.SUBFIELD,
                "data": sub
            }
            return field

        decl_name = decl_type.declname

        # Composits -> advance one lever deeper
        decl_type = decl_type.type

        # This is a typedef'd nested field -> check if its marked as REFLECT(...)
        if isinstance(decl_type, c_ast.IdentifierType):

            if base_type in self.r_structs:
                field["kind"] = "K_STRUCT"
                field["subtype"] = {
                    "kind": SubtypeKind.STRUCT,
                    "data": self.r_structs[base_type],  # ← pointer
                }
                return field

            if base_type in self.r_unions:
                field["kind"] = "K_UNION"
                field["subtype"] = {
                    "kind": SubtypeKind.UNION,
                    "data": self.r_unions[base_type],
                }
                return field

            if base_type in self.r_enums:
                field["kind"] = "K_ENUM"
                field["subtype"] = {
                    "kind": SubtypeKind.ENUM,
                    "data": self.r_enums[base_type],
                }
                return field

            raise RuntimeError(
                f"Unallowed type: {base_type}, at {parent_struct}::{decl_name}"
            )


            
        # Normal nested composite fields
        # Structs
        if isinstance(decl_type, c_ast.Struct):
            if decl_type.name is None: self._err_annon_nested(parent_struct, decl_name)
            field["kind"] = "K_STRUCT"
            field["subtype"] = {
                "kind": SubtypeKind.STRUCT,
                "data": self._process_struct("struct " + decl_type.name, decl_type),
            }
            return field
        
        # Unions
        if isinstance(decl_type, c_ast.Union):
            if decl_type.name is None: self._err_annon_nested(parent_struct, decl_name)
            field["kind"] = "K_UNION"
            field["subtype"] = {
                "kind": SubtypeKind.UNION,
                "data": self._process_struct("union " + decl_type.name, decl_type, union=True),
            }
            return field
        
        # Enums
        if isinstance(decl_type, c_ast.Enum):
            if decl_type.name is None: self._err_annon_nested(parent_struct, decl_name)
            field["kind"] = "K_ENUM"
            field["subtype"] = {
                "kind": SubtypeKind.ENUM,
                "data": self._process_enum("enum " + decl_type.name, decl_type),
            }
            return field
        
        # Fixed-Sixed Arrays
        if isinstance(decl_type, c_ast.ArrayDecl):
            field["kind"] = "K_FIXED_ARRAY"
            field["size"] = decl_type.dim
            field["subtype"] = self._process_field(
                    name=f"{name[:-1]}",
                    offset=0,
                    decl_type=decl_type.type,
                    parent_struct=parent_struct,
                    is_struct=is_struct
                )
            return field



            
    """
    typedef struct {
    const char* typename;
    RField* fields;
    size_t n_elements;
    } RStruct, RUnion;
    """
    def _process_struct(self, typename: str, node: c_ast.Struct, union=False) -> dict[str, any]:
        struct = {
            "typename": typename,
            "fields": [],
            "n_elements": 0
        }

        # extract offsets first
        if not union:
            field_names = []
            for field in node.decls:
                field_names.append(field.name)

            offsets = prober.get_offsets_of_type(self.doth, typename, field_names)

        for idx, field in enumerate(node.decls):
            off = 0
            if not union: off = offsets[idx]
            field_res = self._process_field(
                field.name, off, field.type, typename, not union)

            struct["fields"].append(field_res)
            struct["n_elements"] += 1

            
        if not union: 
            self.r_structs[typename] = struct
        else: 
            self.r_unions[typename] = struct
        return struct

    def _resolve_composite_type(self, decl, parent_struct, field_name):
        """
        Returns (kind, type_name)
        """
        if isinstance(decl, c_ast.Struct):
            kind = "K_STRUCT"
        elif isinstance(decl, c_ast.Union):
            kind = "K_UNION"
        elif isinstance(decl, c_ast.Enum):
            kind = "K_ENUM"
        else:
            return None, None

        # Priority 1: explicit name
        if decl.name:
            return kind, decl.name

        # Priority 2: anonymous → synthesize a stable name
        anon_name = f"{parent_struct}__{field_name}"
        return kind, anon_name
    
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
    

    def _str_type_to_decl(self, type_str: str) -> c_ast.IdentifierType:
        type_str = type_str.strip()

        # pointer
        if type_str.endswith("*"):
            inner = type_str[:-1].strip()
            return c_ast.PtrDecl(
                quals=[],
                type=self._str_type_to_decl(inner)
            )

        # const qualifier (ignore for now)
        if type_str.startswith("const "):
            return self._str_type_to_decl(type_str[6:])

        # base identifier
        return c_ast.IdentifierType([type_str])

    def _is_pointer(self, decl):
        while isinstance(decl, (c_ast.TypeDecl, c_ast.ArrayDecl)):
            decl = decl.type
        return isinstance(decl, c_ast.PtrDecl)


    def _get_base_type(self, decl) -> str | None:
        while True:
            if isinstance(decl, (c_ast.TypeDecl, c_ast.ArrayDecl, c_ast.PtrDecl)):
                decl = decl.type
            elif isinstance(decl, c_ast.IdentifierType):
                return " ".join(decl.names)
            else:
                return None
            
    def _err_annon_nested(self, parent_struct, fieldname):
        raise NameError(f"Annonymous struct/enums/unions are not allowed -> {parent_struct}::{fieldname}, give it a name")



        




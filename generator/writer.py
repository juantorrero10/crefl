

import struct
import parser

VERSION = int(0)
MAGIC = b'CREFLX64'
ENDIANNESS = int(0)
FLAGS = int(0)
CHECKSUM_OFFSET = 13
N_SECTIONS = int(7)
N_HASH_TABLES = int(3)

SIZE_HEADER = 33
SIZE_SECTION_TABLE = 20
SIZE_FIELD = 24
SIZE_STRUCT = 16
SIZE_ENUM = 20


"""
fnv1a hashing algorithm
"""
def fnv1a(s: str) -> int:
    h = 0x811c9dc5
    for b in s.encode("utf-8"):
        h ^= b
        h = (h * 0x01000193) & 0xffffffff
    return h





class CreflWriter:
    def __init__(self, output_path: str, checksum: str, data: list[dict[str, any]]):
        assert(len(checksum) == 16)
        self.output_file = output_path
        self.checksum = checksum
        self.data = data
        self.content = bytearray()

        self.n_structs = 0
        self.n_enums = 0
        self.n_unions = 0
        self.n_fields = 0

        self.structs = bytearray()
        self.unions = bytearray()
        self.enums = bytearray()
        self.fields = bytearray()

        self.string_pool = ""
        self.curr_sp_offset = 0

        self.index_table = bytearray()
        self.curr_it_offset = 0

        # Creating the hash tables
        self.ht_enums:   dict[str, int] = {}
        self.ht_structs: dict[str, int] = {}
        self.ht_unions:  dict[str, int] = {}

        self.cur = 0

    def write(self):
        self.content += self._get_header()
        self._process_sections()
        self.content += self._get_section_tables()
        self.content += self.fields
        self.content += self.enums
        self.content += self.structs
        self.content += self.unions
        self.content += self.string_pool.encode("utf-8")
        self.content += self.index_table
        self.content += self._get_hash_tables()

        with open(self.output_file, "wb") as f:
            f.write(self.content)

    """
    typedef struct CREFL_PACKED
    {
        uint64_t    magic;             // "CREFLX64"
        uint8_t     endianness;        // 00 little endian, 01 big endian
        uint16_t    version;           // 00 00
        uint16_t    flags;             // Maybe useful in the future
        uint8_t     checksum[16];
        uint32_t    n_sections;        // 7
    } CREFL_Header;
    """
    def _get_header(self) -> bytes:
        self.cur += (8 + 1 + 2 + 2 + 16 + 4)
        return struct.pack(
            "<8s B H H 16s I",
            MAGIC,
            ENDIANNESS,
            VERSION,
            FLAGS,
            self.checksum,
            N_SECTIONS,
        )
    
    """
    typedef struct CREFL_PACKED
    {
        CREFL_String name;
        uint32_t offset;            // Runtime offset, nothing to do with the .CREFL
        uint32_t array_size;
        uint32_t kind;              // Same as RKind


        // Its meaning derives from the kind att
        // K_STRUCT, K_UNION, K_ENUM                        -> index of the type into the respective section.
        // K_OPTION, K_FIXED_ARRAY, K_DYN_ARRAY, K_POINTER  -> index of the subfield in the field section
        // K_PRIMITIVE                                      -> Type of primitive, same as KPimitive enum.
        // K_STRING                                         -> Unused
        uint32_t subtype_data;
    } CREFL_Field;

    typedef struct CREFL_PACKED
    {
        CREFL_String type_name;
        // Storing n_elements's DWORD id's of the fields
        index_table_offset fields;
        uint32_t n_elements;
    } CREFL_Struct, CREFL_Union;
    """
    def _process_field(self, field) -> int:
        name = field["name"]
        name_length = len(name)
        self.string_pool += name + '\0'
        name_offset = self.curr_sp_offset
        self.curr_sp_offset += name_length + 1

        offset = field["offset"]
        kind = parser.KIND_VALUES[field["kind"]]
        size = field["size"]

        data = 0

        if field.get("subtype"):
            match field["subtype"]["kind"]:
                case parser.SubtypeKind.SUBFIELD:
                    # Process a subfield
                    data = self._process_field(field["subtype"]["data"])
                case parser.SubtypeKind.STRUCT:
                    data = self._process_struct_union(field["subtype"]["data"], 0)
                case parser.SubtypeKind.UNION:
                    data = self._process_struct_union(field["subtype"]["data"], 1)
                case parser.SubtypeKind.ENUM:
                    data = self._process_enum(field["subtype"]["data"])
                case parser.SubtypeKind.PRIM_TYPE:
                    # process type of primitive
                    data = parser.PRIMITIVE_VALUES[field["subtype"]["data"]]

                case _:
                    raise RuntimeError

        # Push current field index
        self.n_fields += 1


        
        self.fields += struct.pack(
            "<I I I I I I",
            name_offset,
            name_length,
            offset,
            size,
            kind,
            data
        )

        return self.n_fields - 1

    def _process_struct_union(self, st, is_union: bool) -> int:
        name = st["typename"]
        length = len(name)
        self.string_pool += name + '\0'
        str_offset = self.curr_sp_offset
        self.curr_sp_offset += length + 1
        n_elements = int(st["n_elements"])

        ids = []

        # PROCESS AND PUSH FIELDS ONTO THE INDEX TABLE
        for field in st["fields"]:
            ids.append(self._process_field(field))

        proc = struct.pack(
            "<I I I I",
            str_offset,
            length,
            self.curr_it_offset,              # Populated later
            n_elements,
        )

        ret = 0

        if is_union : 
            self.unions += proc
            ret = self.n_unions
            self.ht_unions[name] = ret
            self.n_unions += 1
        else : 
            self.structs += proc
            ret = self.n_structs
            self.ht_structs[name] = ret
            self.n_structs += 1

        # Add the index entries
        self._indextable_grow(len(ids))

        for idx, field in enumerate(st["fields"]):
            struct.pack_into(
                "<I", self.index_table, 
                ((self.curr_it_offset + idx)* 4), 
                ids[idx])
            
        self.curr_it_offset += st["n_elements"]
        return ret


    """
    typedef struct CREFL_PACKED {
        CREFL_String type_name;
        // Referencing n_elements * 2 DWORDS in index_table
        // They are string offset-size pairs
        // For parsing we need to construct the strings using the pairs
        index_table_offset names;
        
        // Referencing the values (indexes) of each enum entry.
        // Taking n_elements DWORD from the table
        index_table_offset indexes;
        uint32_t n_elements;
    } CREFL_Enum;
    """ 
    def _process_enum(self, en):
        name = en["typename"]
        name_off = self.curr_sp_offset
        name_length = len(name)
        self.string_pool += name + '\0'
        self.curr_sp_offset += name_length + 1
        labels = self.curr_it_offset

        # push enums labels
        self._indextable_grow(2 * len(en["names"]))
        for label in en["names"]:
            lbl_length = len(label)
            self.string_pool += label + '\0'
            str_off = self.curr_sp_offset
            self.curr_sp_offset += lbl_length + 1
            struct.pack_into("<I I", self.index_table, (self.curr_it_offset * 4), str_off, lbl_length)
            self.curr_it_offset += 2

        indexes = self.curr_it_offset

        self._indextable_grow(len(en["indexes"]))
        for index in en["indexes"]:
            struct.pack_into("<I", self.index_table, (self.curr_it_offset * 4), index)
            self.curr_it_offset += 1

        proc = struct.pack(
            "<I I I I I",
            name_off,
            name_length,
            labels,
            indexes,
            en["n_elements"]
        )
        self.n_enums += 1
        self.enums += proc

        self.ht_enums[name] = self.n_enums - 1
        return self.n_enums - 1



    def _process_sections(self):
        for st in self.data["Structs"]:
            self._process_struct_union(st, False)

        for en in self.data["Enums"]:
            self._process_enum(en)

        for un in self.data["Unions"]:
            self._process_struct_union(un, True)



    
    """
    typedef struct CREFL_PACKED 
    {
        uint32_t kind;              // Means the same as the enum but it has to be 4 bytes
        uint64_t start;             // From FILE SET
        uint32_t n_elements;
        uint32_t element_size;
    } CREFL_Section_table;
    """
    def _get_section_tables(self) -> bytes:
        count_list = [
            self.n_fields, self.n_enums, 
            self.n_structs, self.n_unions, 
            self.curr_sp_offset, self.curr_it_offset, 
            N_HASH_TABLES]
        
        section_base = SIZE_HEADER + ( SIZE_SECTION_TABLE * N_SECTIONS)
        enums = section_base + (SIZE_FIELD * self.n_fields)
        structs = enums + (SIZE_ENUM * self.n_enums)
        unions = structs + (SIZE_STRUCT * self.n_structs)
        strings = unions + (SIZE_STRUCT * self.n_unions)
        index_tab = strings + self.curr_sp_offset
        hash_tables = index_tab + (self.curr_it_offset * 4)

        elem_sizes = [
            SIZE_FIELD,
            SIZE_ENUM,
            SIZE_STRUCT,
            SIZE_STRUCT,
            1,
            1,
            8
        ]

        starts = [
            section_base,
            enums,
            structs,
            unions,
            strings,
            index_tab,
            hash_tables
        ]

        ret = bytearray()

        for x in range(0, N_SECTIONS):
            ret += struct.pack(
                "<I Q I I",
                x,
                starts[x],
                count_list[x],
                elem_sizes[x]
        )
        return ret
            
    def _indextable_grow(self, size):
        needed_dwords = self.curr_it_offset + size
        needed_bytes = needed_dwords * 4

        if len(self.index_table) < needed_bytes:
            self.index_table.extend(b'\x00' * (needed_bytes - len(self.index_table)))


    """
    typedef struct CREFL_PACKED {
        uint32_t n_elements;           
        uint32_t next_hash_table_offset;  // From start of section
    } CREFL_Hashtable_Header;

    typedef struct CREFL_PACKED
    {
        uint32_t element_id;
        uint32_t hashed_index;
    } CREFL_Hashtable_Entry;
    """
    def _get_hash_tables(self):
        # Theres 3 hashtables for enums, structs, an unions
        # Maybe more in the future
        # Entry hashing with the typename
        tables = [self.ht_enums, self.ht_structs, self.ht_unions]
        counts = [self.n_enums, self.n_structs, self.n_unions]
        hashtables = bytearray()

        size_entry = 8
        size_header = 8
        curr_offset = 0

        for i, table in enumerate(tables):
            entries = bytearray()
            for name, in_file_id in tables[i].items():
                hashed_idx = fnv1a(name) % counts[i] * 2 # Avoid collitions on small lists 
                entries += struct.pack(
                    "<I I",
                    in_file_id,
                    hashed_idx
                )

            if i == N_HASH_TABLES - 1: next_offset = 0
            else: next_offset = curr_offset + size_header + (counts[i] * size_entry)
            header = struct.pack(
                "<I I",
                counts[i],
                next_offset
            )
            curr_offset = next_offset
            hashtables += header
            hashtables += entries

        return hashtables

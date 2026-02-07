

import struct


VERSION = int(0)
MAGIC = b'CREFLX64'
ENDIANNESS = int(0)
FLAGS = int(0)
N_SECTIONS = int(7)
N_HASH_TABLES = int(3)



class CreflWriter:
    def __init__(self, output_path: str, checksum: str, data: list[dict[str, any]]):
        assert(len(checksum) == 16)
        self.output_file = output_path
        self.checksum = checksum
        self.data = data
        self.content = bytearray()

    def write(self):
        self.content += self._get_header()

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
        uint32_t    n_hash_tables;
    } CREFL_Header;
    """
    def _get_header(self) -> bytes:
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
        uint32_t kind;              // Means the same as the enum but it has to be 4 bytes
        uint64_t start;             // From FILE SET
        uint32_t n_elements;
        uint32_t element_size;
    } CREFL_Section_table;
    """
    def _get_section_table(self) -> bytes:
        for i in range():
            pass

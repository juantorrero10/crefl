#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <crefl/types.h>
#include <_fetched.h>
#include <_dot_crefl.h>
#include <crefl/core.h>
#include <_arena.h>

// 1 MB
#define SCRATCH_ARENA_SIZE 1024 * 1024

CREFL_Section_table* sections_tables = NULL;
CREFL_Header* base_ptr = NULL;
CREFL_Field* field_section = NULL;
CREFL_Enum* enum_section = NULL;
CREFL_Struct* struct_section = NULL;
CREFL_Union* union_section = NULL;
uint32_t* index_table = NULL;

size_t NUM_SECTIONS;

static void cleanup();

static void print_header(void* mem, size_t sz) {
    DEREF_NEW(CREFL_Header, header, mem, 0);
    if (header->endianness) {
        fprintf(stderr, "This refl version doesn't support big-endian.\n");
        munmap(mem, sz);
        exit(EXIT_FAILURE);
    }
    else if (__builtin_bswap64(header->magic) != CREFL_MAGIC) {
        fprintf(stderr, "File given is not .CREFL file\n");
        munmap(mem, sz);
        exit(EXIT_FAILURE);
    }

    printf("Header information\n");
    printf("MAGIC: %.*s\n", 8, (char*)&header->magic);
    printf("ENDIANNESS: %hhu\n", header->endianness);
    printf("VERSION: %hu\n", header->version);
    printf("FLAGS: %hu\n", header->flags);
    printf("CHECK_SUM: 0x%lx%lx\n", *(uint64_t*)header->checksum + 8, *(uint64_t*)header->checksum);
    printf("N_SECTIONS: %u\n", header->n_sections);

    NUM_SECTIONS = header->n_sections;
}

static inline void set_string(CREFL_String* desc, char** dst) {
    *dst = G_STRING_POOL + desc->offset;
}

static char* get_st_un_name_from_file_id(uint32_t id, CREFL_Section_kind where) {
    DEREF_NEW(CREFL_Struct, section, base_ptr, sections_tables[where].start);
    return G_STRING_POOL + section[id].type_name.offset;
}

static char* get_en_name_from_file_id(uint32_t id) {
    DEREF_NEW(CREFL_Enum, section, base_ptr, sections_tables[CREFL_SECTION_ENUMS].start);
    return G_STRING_POOL + section[id].type_name.offset;
}

void populate_fields() {
    G_REFL_FIELDS.count = sections_tables[CREFL_SECTION_FIELDS].n_elements;
    for (size_t i = 0; i < sections_tables[CREFL_SECTION_FIELDS].n_elements; i++)
    {
        RField* curr = &G_REFL_FIELDS.fields[i];
        CREFL_Field* ady = &field_section[i];
        *curr = (RField){ 0 };
        set_string(&ady->name, (char**)&curr->name);
        curr->kind = (RKind)ady->kind;
        curr->size = ady->array_size;
        curr->offset = ady->offset;
        curr->next_offset = ady->next_offset;
        switch (curr->kind)
        {
            case K_DYN_ARRAY:
            case K_OPTION:
            case K_POINTER:
            case K_FIXED_ARRAY:
                curr->subtype.subfield = &G_REFL_FIELDS.fields[ady->subtype_data];
                break;

            case K_ENUM:
                curr->subtype.e = refl_fetch_enum(get_en_name_from_file_id(ady->subtype_data)); break;
            case K_STRUCT:
                curr->subtype.s = refl_fetch_struct(get_st_un_name_from_file_id(ady->subtype_data, CREFL_SECTION_STRUCTS));
                break;
            case K_UNION:
                curr->subtype.u = refl_fetch_union(get_st_un_name_from_file_id(ady->subtype_data, CREFL_SECTION_UNIONS));
                break;
            case K_PRIMITIVE:
                curr->subtype.prim = (RPrimitive)ady->subtype_data;
                break;
            default:
                curr->subtype.s = NULL;
        }
    }   
}

void print_field(RField* f, int indent) {
    const char* tabs = "\t\t\t\t\t\t\t\t\t\t\t\t";
    REnum* kind = refl_fetch_enum("RKind");

    if (indent == 0) printf("--------------------\n");
    printf("%.*sName: %s\n",    indent, tabs,   f->name);
    printf("%.*sOffset: %zu\n", indent, tabs,   f->offset);
    printf("%.*sKind: %s\n",    indent, tabs,   refl_enum_get_label(kind, f->kind));
    printf("%.*sSize: %zu\n",   indent, tabs,   f->size);
    if (f->kind == K_PRIMITIVE) {
        REnum* prim = refl_fetch_enum("RPrimitive");
        printf("%.*sPrimitive type: %s\n", indent, tabs, refl_enum_get_label(prim, f->subtype.prim));
    } else if ((int)f->kind == 2 || (int)f->kind >= 6) {
        print_field(f->subtype.subfield, indent + 1);
    }
}

static inline void print_fields(RField* f, size_t sz) {
    for (size_t i = 0; i < sz; i++)
    {
        print_field(&f[i], 0);
    }
}




static void populate_enums() {
    DEREF_NEW(CREFL_Hashtable_Header, enum_ht_header, base_ptr, sections_tables[CREFL_SECTION_HASHTABLES].start);
    size_t n_hash_entries = enum_ht_header->n_elements;
    if (n_hash_entries == 0) return;
    G_REFL_ENUMS.map = calloc(sizeof(refl_en_entry_t), 2 * n_hash_entries);
    G_REFL_ENUMS.count = 2 * n_hash_entries;
    CREFL_Hashtable_Entry* enum_hashtable = (CREFL_Hashtable_Entry*)(enum_ht_header + 1);
    for (size_t i = 0; i < n_hash_entries; i++)
    {
        uint32_t file_idx = enum_hashtable[i].element_id;
        uint32_t new_idx  = enum_hashtable[i].hashed_index;
        CREFL_Enum* ady = enum_section + file_idx;
        refl_en_entry_t* entry = &G_REFL_ENUMS.map[new_idx];
        
        if (entry->k_name != NULL) {
            // Collision!!!, linearly find next empty spot
            do  {
                new_idx = (new_idx + 1) % (2 * n_hash_entries);
                entry = &G_REFL_ENUMS.map[new_idx];
            } while(entry->k_name != NULL);
        }
        
        // Free spot by this point
        REnum* curr = &entry->val;
        set_string(&ady->type_name, (char**)&entry->k_name);
        set_string(&ady->type_name, (char**)&curr->name_type);
        
        // Set the names
        curr->n_elements = ady->n_elements;
        curr->names = calloc(sizeof(char*), ady->n_elements);

        CREFL_String* str_refs = (CREFL_String*)(index_table + ady->names);   
        for (size_t i = 0; i < ady->n_elements; i++)
        {
            curr->names[i] = G_STRING_POOL + str_refs[i].offset;
        }

        curr->indexes = calloc(sizeof(size_t), ady->n_elements);
        uint32_t* file_indexes = (uint32_t*)(index_table + ady->indexes);
        for (size_t i = 0; i < ady->n_elements; i++) {
            curr->indexes[i] = (size_t)file_indexes[i];
        }
    }
}

static void populate_structs() {
    DEREF_NEW(CREFL_Hashtable_Header, ht_header, base_ptr, sections_tables[CREFL_SECTION_HASHTABLES].start);
    
    // find the struct table, 2 second one
    uint8_t* ptr = (uint8_t*)ht_header + ht_header->next_hash_table_offset;
    ht_header = (CREFL_Hashtable_Header*)ptr;

    size_t n_hash_entries = ht_header->n_elements;
    if (!n_hash_entries) return;

    G_REFL_STRUCTS.map = calloc(sizeof(refl_st_entry_t), 2 * n_hash_entries);
    G_REFL_STRUCTS.count = 2 * n_hash_entries;

    CREFL_Hashtable_Entry* struct_ht = (CREFL_Hashtable_Entry*)(ht_header + 1);
    for (size_t i = 0; i < n_hash_entries; i++)
    {
        size_t file_idx = struct_ht[i].element_id;
        size_t new_idx = struct_ht[i].hashed_index;
        CREFL_Struct* ady = &struct_section[file_idx];

        refl_st_entry_t* entry = &G_REFL_STRUCTS.map[new_idx];
        while(entry->k_name != NULL) {
            new_idx = (new_idx + 1) % (2 * n_hash_entries);
            entry = &G_REFL_STRUCTS.map[new_idx];
        }
        RStruct* curr =  &entry->val;

        // Occupy the spot
        set_string(&ady->type_name, (char**)&entry->k_name);
        set_string(&ady->type_name, (char**)&curr->name_type);

        curr->n_elements = ady->n_elements;
        curr->total_size = ady->total_size;
        curr->fields = calloc(sizeof(void*), curr->n_elements);
        for (size_t i = 0; i < curr->n_elements; i++)
        {
            curr->fields[i] = G_REFL_FIELDS.fields + (uint32_t)(index_table + ady->fields)[i];
        }    
    }
}

static void populate_unions() {
    // find the union table, third one
    uint8_t* ht_base =
    (uint8_t*)base_ptr + sections_tables[CREFL_SECTION_HASHTABLES].start;

    /* 1st table */
    CREFL_Hashtable_Header* ht1 =
        (CREFL_Hashtable_Header*)ht_base;

    /* 2nd table */
    CREFL_Hashtable_Header* ht2 =
        (CREFL_Hashtable_Header*)(ht_base + ht1->next_hash_table_offset);

    /* 3rd table (UNIONS) */
    CREFL_Hashtable_Header* ht_header =
        (CREFL_Hashtable_Header*)(ht_base + ht2->next_hash_table_offset);
    
    size_t n_hash_entries = ht_header->n_elements;
    CREFL_Hashtable_Entry* union_ht = (CREFL_Hashtable_Entry*)(ht_header + 1);

    if (!n_hash_entries) return;

    G_REFL_UNIONS.map = calloc(sizeof(refl_un_entry_t), 2 * n_hash_entries);
    G_REFL_UNIONS.count = 2 * n_hash_entries;


                    
    for (size_t i = 0; i < n_hash_entries; i++)
    {
        size_t file_idx = union_ht[i].element_id;
        size_t new_idx = union_ht[i].hashed_index;
        refl_un_entry_t* entry = &G_REFL_UNIONS.map[new_idx];
        while(entry->k_name != NULL) {
            new_idx = (new_idx + 1) % (2 * n_hash_entries);
            entry = &G_REFL_UNIONS.map[new_idx];
        }
        RUnion* curr =  &entry->val;
        CREFL_Union* ady = &union_section[file_idx];

        // Occupy the spot
        set_string(&ady->type_name, (char**)&entry->k_name);
        set_string(&ady->type_name, (char**)&curr->name_type);

        curr->n_elements = ady->n_elements;
        curr->total_size = ady->total_size;
        curr->fields = calloc(sizeof(void*), curr->n_elements);
        for (size_t i = 0; i < curr->n_elements; i++)
        {
            curr->fields[i] = G_REFL_FIELDS.fields + (uint32_t)(index_table + ady->fields)[i];
        }    
    }
}


void refl_engine_load(const char* file_path) {

    int fd = open(file_path, O_RDONLY, 0);
    if (fd < 0) {
        printf("Refl engine: Could not open file '%s': %s\n", file_path, strerror(errno));
        exit(EXIT_FAILURE);
    }
    struct stat st;
    fstat(fd, &st);
    size_t size = st.st_size;


    void* mapped = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);

    (void)print_header;
    //print_header(mapped, size);
    DEREF(CREFL_Header,         base_ptr, mapped, 0);
    DEREF(CREFL_Section_table,  sections_tables, mapped, sizeof(CREFL_Header));
    DEREF(CREFL_Field,          field_section, mapped, sections_tables[CREFL_SECTION_FIELDS].start);
    DEREF(CREFL_Enum,           enum_section, mapped, sections_tables[CREFL_SECTION_ENUMS].start);
    DEREF(CREFL_Struct,         struct_section, mapped, sections_tables[CREFL_SECTION_STRUCTS].start);
    DEREF(CREFL_Union,          union_section, mapped, sections_tables[CREFL_SECTION_UNIONS].start);
    DEREF(uint32_t,             index_table, mapped, sections_tables[CREFL_SECTION_INDEX_TABLE].start);

    size_t sp_size = sections_tables[CREFL_SECTION_STRING_POOL].n_elements;
    G_STRING_POOL = malloc(sp_size);

    memcpy(G_STRING_POOL, (uint8_t*)mapped + (sections_tables[CREFL_SECTION_STRING_POOL].start), sp_size);
    // Fields need to be allocated before anything alse
    G_REFL_FIELDS.fields = malloc(sizeof(RField) * sections_tables[CREFL_SECTION_FIELDS].n_elements);

    populate_enums();
    populate_structs();
    populate_unions();
    populate_fields();
    //print_fields(G_REFL_FIELDS.fields, G_REFL_FIELDS.count);


    munmap(mapped, size);

    // ALLOC SCRATCH ARENA
    arena_init(SCRATCH_ARENA_SIZE);

    atexit(cleanup);
}


static void cleanup() {
    free(G_REFL_FIELDS.fields);
    for (size_t i = 0; i < G_REFL_ENUMS.count; i++)
    {
        free(G_REFL_ENUMS.map[i].val.names);
        free(G_REFL_ENUMS.map[i].val.indexes);
    }
    for (size_t i = 0; i < G_REFL_STRUCTS.count; i++)
    {
        free(G_REFL_STRUCTS.map[i].val.fields);
    }
    
    for (size_t i = 0; i < G_REFL_UNIONS.count; i++)
    {
        free(G_REFL_UNIONS.map[i].val.fields);
    }
    
    free(G_REFL_STRUCTS.map);
    free(G_REFL_UNIONS.map);
    free(G_REFL_ENUMS.map);
    free(G_STRING_POOL);
}
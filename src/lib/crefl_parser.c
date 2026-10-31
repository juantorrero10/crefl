#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <public/types.h>
#include <public/scope.h>
#include <public/core.h>

#include <private/metadata.h>
#include <private/crefl_file.h>
#include <private/arena.h>


CREFL_Section_table* sections_tables = NULL;
CREFL_Header* base_ptr = NULL;
CREFL_Field* field_section = NULL;
CREFL_Enum* enum_section = NULL;
CREFL_Struct* struct_section = NULL;
CREFL_Union* union_section = NULL;
u32* index_table = NULL;

typedef struct {
    refl_st_hashmap_t structs;
    refl_en_hashmap_t enums;
    refl_un_hashmap_t unions;
    refl_fi_array_t fields;
    char* string_pool;
} mdata_table_set;

size_t NUM_SECTIONS;


static void print_header(void* mem, size_t sz) {
    DEREF_NEW(CREFL_Header, header, mem, 0);
    if (header->flags1 & 1) {
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
    printf("VERSION: %hu\n", header->version);
    printf("FLAGS1: %hhu\n", header->flags1);
    printf("FLAGS2: %hhu\n", header->flags2);
    printf("CHECK_SUM: 0x%lx%lx\n", *(u64*)header->checksum + 8, *(u64*)header->checksum);
    printf("N_SECTIONS: %u\n", header->n_sections);

    NUM_SECTIONS = header->n_sections;
}

static inline void set_string(char* string_pool, CREFL_String* desc, char** dst) {
    *dst = string_pool + desc->offset;
}

static char* get_st_un_name_from_file_id(char* string_pool, u32 id, CREFL_Section_kind where) {
    DEREF_NEW(CREFL_Struct, section, base_ptr, sections_tables[where].start);
    return string_pool + section[id].type_name.offset;
}

static char* get_en_name_from_file_id(char* string_pool, u32 id) {
    DEREF_NEW(CREFL_Enum, section, base_ptr, sections_tables[CREFL_SECTION_ENUMS].start);
    return string_pool + section[id].type_name.offset;
}

void populate_fields(mdata_table_set* table) {
    table->fields.count = sections_tables[CREFL_SECTION_FIELDS].n_elements;
    for (size_t i = 0; i < sections_tables[CREFL_SECTION_FIELDS].n_elements; i++)
    {
        RField* curr = &table->fields.fields[i];
        CREFL_Field* ady = &field_section[i];
        *curr = (RField){ 0 };
        set_string(table->string_pool, &ady->name, (char**)&curr->name);
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
                curr->subtype.subfield = &table->fields.fields[ady->subtype_data];
                break;

            case K_ENUM:
                curr->subtype.e = refl_fetch_enum_from(table, get_en_name_from_file_id(table->string_pool, ady->subtype_data)); break;
            case K_STRUCT:
                curr->subtype.s = refl_fetch_struct_from(table, get_st_un_name_from_file_id(table->string_pool, ady->subtype_data, CREFL_SECTION_STRUCTS));
                break;
            case K_UNION:
                curr->subtype.u = refl_fetch_union_from(table, get_st_un_name_from_file_id(table->string_pool, ady->subtype_data, CREFL_SECTION_UNIONS));
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




static void populate_enums(refl_en_hashmap_t* enum_hm, char* string_pool) {
    DEREF_NEW(CREFL_Hashtable_Header, enum_ht_header, base_ptr, sections_tables[CREFL_SECTION_HASHTABLES].start);
    size_t n_hash_entries = enum_ht_header->n_elements;
    if (n_hash_entries == 0) return;
    enum_hm->map = calloc(sizeof(refl_en_entry_t), 2 * n_hash_entries);
    enum_hm->count = 2 * n_hash_entries;
    CREFL_Hashtable_Entry* enum_hashtable = (CREFL_Hashtable_Entry*)(enum_ht_header + 1);
    for (size_t i = 0; i < n_hash_entries; i++)
    {
        u32 file_idx = enum_hashtable[i].element_id;
        u32 new_idx  = enum_hashtable[i].hashed_index;
        CREFL_Enum* ady = enum_section + file_idx;
        refl_en_entry_t* entry = &enum_hm->map[new_idx];
        
        if (entry->k_name != NULL) {
            // Collision!!!, linearly find next empty spot
            do  {
                new_idx = (new_idx + 1) % (2 * n_hash_entries);
                entry = &enum_hm->map[new_idx];
            } while(entry->k_name != NULL);
        }
        
        // Free spot by this point
        REnum* curr = &entry->val;
        set_string(string_pool, &ady->type_name, (char**)&entry->k_name);
        set_string(string_pool, &ady->type_name, (char**)&curr->name_type);
        
        // Set the names
        curr->n_elements = ady->n_elements;
        curr->names = calloc(sizeof(char*), ady->n_elements);

        CREFL_String* str_refs = (CREFL_String*)(index_table + ady->names);   
        for (size_t i = 0; i < ady->n_elements; i++)
        {
            curr->names[i] = string_pool + str_refs[i].offset;
        }

        curr->indexes = calloc(sizeof(size_t), ady->n_elements);
        u32* file_indexes = (u32*)(index_table + ady->indexes);
        for (size_t i = 0; i < ady->n_elements; i++) {
            curr->indexes[i] = (size_t)file_indexes[i];
        }
    }
}

static void populate_structs(refl_st_hashmap_t* struct_hm, refl_fi_array_t* field_arr, char* string_pool) {
    DEREF_NEW(CREFL_Hashtable_Header, ht_header, base_ptr, sections_tables[CREFL_SECTION_HASHTABLES].start);
    
    // find the struct table, 2 second one
    u8* ptr = (u8*)ht_header + ht_header->next_hash_table_offset;
    ht_header = (CREFL_Hashtable_Header*)ptr;

    size_t n_hash_entries = ht_header->n_elements;
    if (!n_hash_entries) return;

    struct_hm->map = calloc(sizeof(refl_st_entry_t), 2 * n_hash_entries);
    struct_hm->count = 2 * n_hash_entries;

    CREFL_Hashtable_Entry* struct_ht = (CREFL_Hashtable_Entry*)(ht_header + 1);
    for (size_t i = 0; i < n_hash_entries; i++)
    {
        size_t file_idx = struct_ht[i].element_id;
        size_t new_idx = struct_ht[i].hashed_index;
        CREFL_Struct* ady = &struct_section[file_idx];

        refl_st_entry_t* entry = &struct_hm->map[new_idx];
        while(entry->k_name != NULL) {
            new_idx = (new_idx + 1) % (2 * n_hash_entries);
            entry = &struct_hm->map[new_idx];
        }
        RStruct* curr =  &entry->val;

        // Occupy the spot
        set_string(string_pool, &ady->type_name, (char**)&entry->k_name);
        set_string(string_pool, &ady->type_name, (char**)&curr->name_type);

        curr->n_elements = ady->n_elements;
        curr->total_size = ady->total_size;
        curr->fields = calloc(sizeof(void*), curr->n_elements);
        for (size_t i = 0; i < curr->n_elements; i++)
        {
            curr->fields[i] = field_arr->fields + (u32)(index_table + ady->fields)[i];
        }    
    }
}

static void populate_unions(refl_un_hashmap_t* union_hm, refl_fi_array_t* field_arr, char* string_pool) {
    // find the union table, third one
    u8* ht_base =
    (u8*)base_ptr + sections_tables[CREFL_SECTION_HASHTABLES].start;

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

    union_hm->map = calloc(sizeof(refl_un_entry_t), 2 * n_hash_entries);
    union_hm->count = 2 * n_hash_entries;


                    
    for (size_t i = 0; i < n_hash_entries; i++)
    {
        size_t file_idx = union_ht[i].element_id;
        size_t new_idx = union_ht[i].hashed_index;
        refl_un_entry_t* entry = &union_hm->map[new_idx];
        while(entry->k_name != NULL) {
            new_idx = (new_idx + 1) % (2 * n_hash_entries);
            entry = &union_hm->map[new_idx];
        }
        RUnion* curr =  &entry->val;
        CREFL_Union* ady = &union_section[file_idx];

        // Occupy the spot
        set_string(string_pool, &ady->type_name, (char**)&entry->k_name);
        set_string(string_pool, &ady->type_name, (char**)&curr->name_type);

        curr->n_elements = ady->n_elements;
        curr->total_size = ady->total_size;
        curr->fields = calloc(sizeof(void*), curr->n_elements);
        for (size_t i = 0; i < curr->n_elements; i++)
        {
            curr->fields[i] = field_arr->fields + (u32)(index_table + ady->fields)[i];
        }    
    }
}


void _parse_crefl(
    const char* file_path, 
    mdata_table_set* table
) {


    int fd = open(file_path, O_RDONLY, 0);
    if (fd < 0) {
        printf("Refl engine: Could not open file '%s': %s\n", file_path, strerror(errno));
        exit(EXIT_FAILURE);
    }
    struct stat st;
    fstat(fd, &st);
    size_t size = st.st_size;

    sections_tables = NULL;
    base_ptr        = NULL;
    field_section   = NULL;
    enum_section    = NULL;
    struct_section  = NULL;
    union_section   = NULL;
    index_table     = NULL;


    void* mapped = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);

    (void)print_header;
    //print_header(mapped, size);
    DEREF(CREFL_Header,         base_ptr, mapped, 0);
    DEREF(CREFL_Section_table,  sections_tables, mapped, sizeof(CREFL_Header));
    DEREF(CREFL_Field,          field_section, mapped, sections_tables[CREFL_SECTION_FIELDS].start);
    DEREF(CREFL_Enum,           enum_section, mapped, sections_tables[CREFL_SECTION_ENUMS].start);
    DEREF(CREFL_Struct,         struct_section, mapped, sections_tables[CREFL_SECTION_STRUCTS].start);
    DEREF(CREFL_Union,          union_section, mapped, sections_tables[CREFL_SECTION_UNIONS].start);
    DEREF(u32,             index_table, mapped, sections_tables[CREFL_SECTION_INDEX_TABLE].start);

    size_t sp_size = sections_tables[CREFL_SECTION_STRING_POOL].n_elements;
    table->string_pool = malloc(sp_size);

    memcpy(table->string_pool, (u8*)mapped + (sections_tables[CREFL_SECTION_STRING_POOL].start), sp_size);
    // Fields need to be allocated before anything alse
    table->fields.fields = malloc(sizeof(RField) * sections_tables[CREFL_SECTION_FIELDS].n_elements);

    populate_enums  (&table->enums, table->string_pool);
    populate_structs(&table->structs, &table->fields, table->string_pool);
    populate_unions (&table->unions, &table->fields, table->string_pool);
    populate_fields (table);
    //print_fields(G_REFL_FIELDS.fields, G_REFL_FIELDS.count);


    munmap(mapped, size);
    close(fd);
}


static void _destroy_scope(mdata_table_set* scope) {
    free(scope->fields.fields);
    if (scope->enums.map) {
        for (size_t i = 0; i < scope->enums.count; i++)
        {
            free(scope->enums.map[i].val.names);
            free(scope->enums.map[i].val.indexes);
        }
    }
    if (scope->structs.map) {
        for (size_t i = 0; i < scope->structs.count; i++)
        {
            free(scope->structs.map[i].val.fields);
        }
        
    }
    if (scope->unions.map) {
        for (size_t i = 0; i < scope->unions.count; i++)
        {
            free(scope->unions.map[i].val.fields);
        }
    }
    
    free(scope->structs.map);
    free(scope->unions.map);
    free(scope->enums.map);
    free(scope->string_pool);
}

static void __destroy_global_scope() {
    mdata_table_set t = {0};
    t.enums = G_REFL_ENUMS;
    t.structs = G_REFL_STRUCTS;
    t.fields = G_REFL_FIELDS;
    t.unions = G_REFL_UNIONS;
    t.string_pool = G_STRING_POOL;
    _destroy_scope(&t);
}

/**
 * @brief Parse .crefl file and store types globally. 
 */
void refl_load_metadata(const char* file_name) {
    mdata_table_set t = {0};
    _parse_crefl(
        file_name, 
        &t
    );
    G_REFL_STRUCTS = t.structs;
    G_REFL_ENUMS = t.enums;
    G_REFL_UNIONS = t.unions;
    G_REFL_FIELDS = t.fields;
    G_STRING_POOL = t.string_pool;
    atexit(__destroy_global_scope);
}

RHandle refl_load_metadata_to(char* file_name) {
    mdata_table_set* t = calloc(1, sizeof(mdata_table_set));
    _parse_crefl(
        file_name,
        t
    );
    return (RHandle)(void*)t;
}

void refl_close_scope(RHandle scope) {
    mdata_table_set* t = (mdata_table_set*)(void*)scope;
    _destroy_scope(t);
    free(t);
}


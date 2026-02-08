#include <string.h>
#include <stdio.h>


#include <public/core.h>
#include <fetched.h>

refl_st_hashmap_t G_REFL_STRUCTS = { 0 };
refl_un_hashmap_t G_REFL_UNIONS = { 0 };
refl_en_hashmap_t G_REFL_ENUMS = { 0 };
refl_fi_array_t G_REFL_FIELDS = { 0 };
char* G_STRING_POOL = NULL;



static inline uint32_t fnv1a(const char* name, size_t sz) {
    uint32_t h = 0x811c9dc5;
    for (size_t i = 0; i < sz; i++)
    {
        uint8_t b = name[i];
        h = (h * 0x01000193) & 0xffffffff;
        h ^= b;
    }
    
    return h;
}

REnum* refl_fetch_enum(const char* name) {
    uint32_t att_count = 0;
    uint32_t idx = fnv1a(name, strlen(name)) % G_REFL_ENUMS.count;
    while(((G_REFL_ENUMS.map[idx].k_name == NULL) 
    || (strcmp(name, G_REFL_ENUMS.map[idx].k_name) != 0)) && att_count <= G_REFL_ENUMS.count ) {
        att_count++;
        idx = (idx + 1) % G_REFL_ENUMS.count;
    }
    return (att_count <= G_REFL_ENUMS.count)? &G_REFL_ENUMS.map[idx].val : NULL;
}


RStruct* refl_fetch_struct(const char* name) {


    uint32_t att_count = 0;
    uint32_t idx = fnv1a(name, strlen(name)) % G_REFL_STRUCTS.count;
    while(((G_REFL_STRUCTS.map[idx].k_name == NULL) 
    || (strcmp(name, G_REFL_STRUCTS.map[idx].k_name) != 0)) && att_count <= G_REFL_STRUCTS.count ) {
        att_count++;
        idx = (idx + 1) % G_REFL_STRUCTS.count;
    }
    return (att_count <= G_REFL_STRUCTS.count)? &G_REFL_STRUCTS.map[idx].val : NULL;
}


RUnion* refl_fetch_union(const char* name) {
    uint32_t att_count = 0;
    uint32_t idx = fnv1a(name, strlen(name)) % G_REFL_UNIONS.count;
    while(((G_REFL_UNIONS.map[idx].k_name == NULL) 
    || (strcmp(name, G_REFL_UNIONS.map[idx].k_name) != 0)) && att_count <= G_REFL_UNIONS.count ) {
        att_count++;
        idx = (idx + 1) % G_REFL_UNIONS.count;
    }
    return (att_count <= G_REFL_UNIONS.count)? &G_REFL_UNIONS.map[idx].val : NULL;
}

const char* refl_enum_strname(REnum* type, uint32_t val) {
    // Straight correlation
    if (val < type->n_elements && type->indexes[val] == val) return type->names[val];
    // Binary search
    int32_t low = 0;
    int32_t high = type->n_elements - 1;

    while (low <= high) {
        int32_t mid = low + (high - low) / 2;
        uint32_t val_at = type->indexes[mid];

        if (val_at == val) {
            return type->names[mid];
        } else if (val_at < val) {
            low = mid + 1;
        } else {
            high = mid - 1;
        }
    }

    return NULL; // not found
}

size_t refl_prim_to_size(RPrimitive p) {
    switch (p)
    {
    case P_U0:
        return 0;
    case P_BOOL:
        return sizeof(int);
    case P_F32:
    case P_I32:
    case P_U32:
        return 4;
    case P_F64:
    case P_I64:
    case P_U64:
        return 8;
    case P_U16:
    case P_I16:
        return 2;
    case P_U8:
    case P_I8:
        return 1;
    
    default:
        break;
    }
    return (size_t)-1;
}

RObject refl_get_field_value(RStruct* st, RObject instance, int idx) {
    return *(void**)((uint8_t*)instance + st->fields[idx]->offset);
}
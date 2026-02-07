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


RStruct* refl_fetch_struct(const char* name) {
    size_t idx = fnv1a(name, strlen(name)) % G_REFL_STRUCTS.count;
    return &G_REFL_STRUCTS.map[idx].val;
}

REnum* refl_fetch_enum(const char* name) {
    uint32_t idx = fnv1a(name, strlen(name)) % G_REFL_ENUMS.count;
    while((G_REFL_ENUMS.map[idx].k_name == NULL) 
    || (strcmp(name, G_REFL_ENUMS.map[idx].k_name) != 0)) {
        idx = (idx + 1) % G_REFL_ENUMS.count;
    }
    return &G_REFL_ENUMS.map[idx].val;
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


RObject refl_fetch_from_instance(RStruct* refl_struct, size_t field_index, RObject instance) {
    if (!refl_struct || field_index >= refl_struct->n_elements) {
        return NULL;
    }
    
    RField* field = &refl_struct->fields[field_index];
    
    return (uint8_t*)instance + field->offset;
}

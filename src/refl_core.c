#include <public/core.h>
#include <fetched.h>

refl_st_hashmap_t G_REFL_STRUCTS = { 0 };
refl_un_hashmap_t G_REFL_UNIONS = { 0 };
refl_en_hashmap_t G_REFL_ENUMS = { 0 };
refl_fi_array_t G_REFL_FIELDS = { 0 };
char* G_STRING_POOL = NULL;


static size_t sthm_hash(const char* name) {
    size_t hash = 5381;
    int c;
    while ((c = *name++))
        hash = ((hash << 5) + hash) + c;
    return hash;
}

RStruct* refl_fetch_struct(const char* name) {
    size_t idx = sthm_hash(name) % G_REFL_STRUCTS.count;
    return &G_REFL_STRUCTS.map[idx].val;
}


RObject refl_fetch_from_instance(RStruct* refl_struct, size_t field_index, RObject instance) {
    if (!refl_struct || field_index >= refl_struct->n_elements) {
        return NULL;
    }
    
    RField* field = &refl_struct->fields[field_index];
    
    return (uint8_t*)instance + field->offset;
}

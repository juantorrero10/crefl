#define _GNU_SOURCE

#include <string.h>
#include <stdio.h>
#include <stdlib.h>


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
        h ^= b;
        h = (h * 0x01000193) & 0xffffffff;
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

int refl_enum_has_label(REnum* e, const char* lbl) {
    if (!lbl) return 0;
    for (size_t i = 0; i < e->n_elements; i++)
    {
        if (strcmp(lbl, e->names[i]) == 0) return 1;
    }
    return 0;
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

void refl_set_field(RStruct* s, const char* field_name, RObject instance, void* buff) {
    if (!s || !field_name || !buff || !instance ) return;
    RField* f = refl_field_by_name(s, field_name);
    if (!f) return;
    memcpy((uint8_t*)instance + f->offset, buff, refl_get_field_size(f));
}

RObject refl_get_field(RStruct* s, const char* field_name, RObject instance) {
    if (!s || !field_name || !instance ) return NULL;
    RField* f = refl_field_by_name(s, field_name);
    if (!f) return f;
    size_t sz = refl_get_field_size(f);
    void* ret = calloc(1, sz);
    memcpy(ret, instance + f->offset, refl_get_field_size(f));
    return ret;
}


void refl_destroy_enum(REnum* owned) {
    if (!owned) return;
    free(owned->indexes);
    for (size_t i = 0; i < owned->n_elements; i++)
    {
        free((void*)owned->names[i]);
    }
    free(owned->names);
    free((void*)owned->name_type);
    free(owned);
}

/**
 * @brief Return a REnum* reflected type with an independent lifetime.
 * Because of this it needs to be freed with refl_destroy_enum() 
 */
REnum* refl_own_enum(REnum* ref) {
    REnum* ret = malloc(sizeof(REnum));
    ret->n_elements = ref->n_elements;
    ret->name_type = strdup(ref->name_type);
    ret->indexes = malloc(ret->n_elements * sizeof(size_t));
    memcpy(ret->indexes, ref->indexes, sizeof(size_t) * ret->n_elements);
    ret->names = malloc(sizeof(char*) * ref->n_elements);
    for (size_t i = 0; i < ret->n_elements; i++)
    {
        ret->names[i] = strdup(ref->names[i]);
    }
    return ret;
}

RStruct* refl_own_struct(RStruct* ref);

RField* refl_own_field(RField* ref) {
    RField* f = malloc(sizeof(RField));
    f->kind = ref->kind;
    f->name = strdup(ref->name);
    f->next_offset = ref->next_offset;
    f->size = ref->size;
    f->offset = ref->offset;
    switch (f->kind)
    {
    case K_PRIMITIVE:
        f->subtype.prim = ref->subtype.prim;
        break;
        break;
    case K_OPTION:
    case K_DYN_ARRAY:
    case K_FIXED_ARRAY:
    case K_POINTER:
        f->subtype.subfield = refl_own_field(ref->subtype.subfield);
        break;
    case K_STRUCT:
    case K_UNION:
        f->subtype.s = refl_own_struct(ref->subtype.s);
        break;
    case K_ENUM:
        f->subtype.e = refl_own_enum(ref->subtype.e);
        break;
    
    default:
        break;
    }

    return f;
}

RStruct* refl_own_struct(RStruct* ref) {
    RStruct* ret = malloc(sizeof(RStruct));

    ret->n_elements = ref->n_elements;
    ret->name_type = strdup(ref->name_type);
    ret->total_size = ref->total_size;

    ret->fields = calloc(sizeof(RField*), ref->n_elements);

    for (size_t i = 0; i < ret->n_elements; i++)
    {
        ret->fields[i] = refl_own_field(ref->fields[i]);
    }
    return ret;
}

void refl_destroy_struct(RStruct* rs);

void refl_destroy_field(RField* owned) {
    if (!owned) return;
    free((char*)owned->name);
    switch (owned->kind)
    {
    case K_DYN_ARRAY:
    case K_FIXED_ARRAY:
    case K_OPTION:
    case K_POINTER:
        refl_destroy_field(owned->subtype.subfield);
        break;
    case K_STRING:
    case K_UNION:
        refl_destroy_struct(owned->subtype.s);
        break;
    case K_ENUM:
        refl_destroy_enum(owned->subtype.e);
        break;
    
    default:
        break;
    }
    free(owned);
    
}

void refl_destroy_struct(RStruct* owned) {
    if (!owned) return;
    free((char*)owned->name_type);
    for (size_t i = 0; i < owned->n_elements; i++)
    {
        refl_destroy_field(owned->fields[i]);
    }
    free(owned->fields);
    free(owned);
}
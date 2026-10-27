#define _GNU_SOURCE

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>


#include <crefl/core.h>
#include <crefl/types.h>
#include <crefl/scope.h>
#include <_fetched.h>
#include <_arena.h>

refl_st_hashmap_t G_REFL_STRUCTS = { 0 };
refl_un_hashmap_t G_REFL_UNIONS = { 0 };
refl_en_hashmap_t G_REFL_ENUMS = { 0 };
refl_fi_array_t G_REFL_FIELDS = { 0 };
char* G_STRING_POOL = NULL;

typedef struct {
    refl_st_hashmap_t structs;
    refl_en_hashmap_t enums;
    refl_un_hashmap_t unions;
    refl_fi_array_t fields;
    char* string_pool;
} mdata_table_set;



static inline u32 fnv1a(const char* name, size_t sz) {
    u32 h = 0x811c9dc5;
    for (size_t i = 0; i < sz; i++)
    {
        u8 b = name[i];
        h ^= b;
        h = (h * 0x01000193) & 0xffffffff;
    }
    
    return h;
}

// 1 MB
#define SCRATCH_ARENA_SIZE 1024 * 1024


void refl_threadlocal_init(int manual_dealloc) {
    // ALLOC SCRATCH ARENA
    arena_init(SCRATCH_ARENA_SIZE, manual_dealloc);
}

void refl_threadlocal_destroy() { arena_tls_free(); }


REnum* refl_fetch_enum_from(RHandle scope, const char* name) {
    mdata_table_set* t = (mdata_table_set*)scope;
    u32 att_count = 0;
    u32 idx = fnv1a(name, strlen(name)) % t->enums.count;
    while(((t->enums.map[idx].k_name == NULL) 
    || (strcmp(name, t->enums.map[idx].k_name) != 0)) && att_count <= t->enums.count ) {
        att_count++;
        idx = (idx + 1) % t->enums.count;
    }
    return (att_count <= t->enums.count)? &t->enums.map[idx].val : NULL;
}

REnum* refl_fetch_enum(const char* name) {
    mdata_table_set t = {0};
    t.enums = G_REFL_ENUMS;
    return refl_fetch_enum_from((void*)&t, name);
}


RStruct* refl_fetch_struct_from(RHandle scope, const char* name) {
    mdata_table_set* t = (mdata_table_set*)scope;
    u32 att_count = 0;
    u32 idx = fnv1a(name, strlen(name)) % t->structs.count;
    while(((t->structs.map[idx].k_name == NULL) 
    || (strcmp(name, t->structs.map[idx].k_name) != 0)) && att_count <= t->structs.count ) {
        att_count++;
        idx = (idx + 1) % t->structs.count;
    }
    return (att_count <= t->structs.count)? &t->structs.map[idx].val : NULL;
}

RStruct* refl_fetch_struct(const char* name) {
    mdata_table_set t = {0};
    t.structs = G_REFL_STRUCTS;
    return refl_fetch_struct_from((void*)&t, name);
}

RUnion* refl_fetch_union_from(RHandle scope, const char* name) {
    mdata_table_set* t = (mdata_table_set*)scope;
    u32 att_count = 0;
    u32 idx = fnv1a(name, strlen(name)) % t->unions.count;
    while(((t->unions.map[idx].k_name == NULL) 
    || (strcmp(name, t->unions.map[idx].k_name) != 0)) && att_count <= t->unions.count ) {
        att_count++;
        idx = (idx + 1) % t->unions.count;
    }
    return (att_count <= t->unions.count)? &t->unions.map[idx].val : NULL;
}

RUnion* refl_fetch_union(const char* name) {
    mdata_table_set t = {0};
    t.unions = G_REFL_UNIONS;
    return refl_fetch_union_from((void*)&t, name);
}


int refl_enum_has_label(REnum* e, const char* lbl) {
    if (!lbl) return 0;
    for (size_t i = 0; i < e->n_elements; i++)
    {
        if (strcmp(lbl, e->names[i]) == 0) return 1;
    }
    return 0;
}





void refl_set_field(RStruct* s, const char* field_name, RObject instance, void* buff) {
    if (!s || !field_name || !buff || !instance ) return;
    RField* f = refl_field_by_name(s, field_name, NULL);
    if (!f) return;
    memcpy((u8*)instance + f->offset, buff, refl_get_field_size(f));
}

RObject refl_get_field(RStruct* s, const char* field_name, RObject instance) {
    if (!s || !field_name || !instance ) return NULL;
    RField* f = refl_field_by_name(s, field_name, NULL);
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
    case K_STRUCT:
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

static void __struct_generic_destructor(RStruct* t, RObject instance, int free_father);
static void __field_generic_destructor(RField* f, void* src);

static void __array_generic_destructor(RField* arr, void* src) {
    int dyn = arr->kind == K_DYN_ARRAY;
    size_t count = arr->size;
    u8* elements = src;

    if (dyn) {
        r_array_t* a = (r_array_t*)src;
        count = a->size;
        elements = a->array;
    }

    size_t stride = refl_get_field_size(arr->subtype.subfield);
    for (size_t i = 0; i < count; i++) {
        __field_generic_destructor(
            arr->subtype.subfield,
            elements + (stride * i)
        );
    }

    if (dyn) free(((r_array_t*)src)->array);
}

static void __field_generic_destructor(RField* f, void* src) {
    switch (f->kind)
    {
    case K_DYN_ARRAY:
    case K_FIXED_ARRAY:
        __array_generic_destructor(f, src);
        break;

    case K_OPTION:
        if (refl_option_is_some(src)) {
            __field_generic_destructor(
                f->subtype.subfield,
                refl_option_data(f, src)
            );
        }
        break;

    case K_STRING:
        free(*(char**)src);
        break;
    case K_STRUCT:
        __struct_generic_destructor(f->subtype.s, src, 0);
        break;
    default:
        break;
    }
}

static void __struct_generic_destructor(RStruct* t, RObject instance, int free_father) {

    for (size_t i = 0; i < t->n_elements; i++)
    {
        RField* f = t->fields[i];
        __field_generic_destructor(f, instance + f->offset);
    }
    if (free_father)free(instance);
}

void refl_generic_destructor(RStruct* t, RObject instance) {
    __struct_generic_destructor(t, instance, 1);
}


_RObject refl_generic_constructor(RStruct* t, ...) {
    va_list params;
    va_start(params, t);

    va_end(params);
    return (_RObject){.type = t, .instance = NULL};
}


RObject refl_object_deepcopy(RStruct* t, const RObject instance) {
    RObject out = malloc(t->total_size);
    memcpy(out, instance, sizeof(t->total_size));
    for (size_t i = 0; i < t->n_elements; i++)
    {
        RField* f = t->fields[i];
        u8* src = ((u8*)instance + f->offset);
        u8* dst = ((u8*)out + f->offset);
        switch (f->kind)
        {
        case K_STRING:
            *(char**)dst = strdup(*(char**)src);
            break;
        case K_DYN_ARRAY: {
            r_array_t* arr = (r_array_t*)src; 
            void* new_arr = malloc(arr->chunk_size * arr->size);
            ((r_array_t*)dst)->array = new_arr;
            break;
        }        
    
        default:
            break;
        }
    }
    return out;
}
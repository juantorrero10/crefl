#define _GNU_SOURCE

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdbool.h>

#include <crefl/core.h>
#include <crefl/types.h>
#include <crefl/cmp.h>


static int refl_primitive_can_promote(RPrimitive a, RPrimitive b);

// Compatibily by label subset
// True if every label in the tinier enum is present in the larger one
int refl_comp_enum_label_subset(REnum* a, REnum* b) {
    REnum* large,* tiny;
    if ((a == NULL)^(b == NULL)) return 0;
    else if (a == NULL) return 1;
    if (a->name_type == b->name_type) return 1;
    if (a->n_elements >= b->n_elements) {
        tiny = b;
        large = a;
    } else {
        tiny = a;
        large = b;
    }

    for (size_t i = 0; i < tiny->n_elements; i++)
    {
        if (!refl_enum_has_label(large, tiny->names[i])) {
            return 0;
        }
    }
    return 1;
}

int refl_comp_field_nametype(RField* a, RField* b) {
    if ((a == NULL)^(b == NULL)) return 0;
    if (strcmp(a->name, b->name) != 0) return 0;
    if (a->kind != b->kind) return 0;
    switch (a->kind)
    {
        case K_PRIMITIVE:
            if (!refl_primitive_can_promote(a->subtype.prim, b->subtype.prim))
                return 0;
            break;
        case K_FIXED_ARRAY:
        case K_POINTER:
        case K_DYN_ARRAY:
        case K_OPTION:
            if (!refl_comp_field_nametype(a->subtype.subfield, b->subtype.subfield)) return 0;
            break;
        case K_STRUCT:
        case K_UNION:
            if (!refl_comp_struct_subset(a->subtype.s, b->subtype.s)) return 0;
            break;
        case K_ENUM:
            // Comparing recursed enums allowing a subset is rarely useful and extrimely inneficient
            // It is still posible to do, just do it manually calling refl_comp_enum_label_subset()
            if (strcmp(a->subtype.e->name_type, b->subtype.e->name_type)) return 0;
            break;
        case K_STRING: break;
    }

    return 1;
}

int refl_comp_struct_subset(RStruct* tiny, RStruct* large) {
    if (!large^!tiny) return 0;
    if (!tiny) return 1;
    if (strcmp(tiny->name_type, large->name_type) == 0) return 1;
    for (size_t i = 0; i < tiny->n_elements; i++)
    {
        RField* f = tiny->fields[i];
        if (f->kind == K_OPTION) continue;
        RField* from_b = refl_field_by_name(large, f->name, NULL);
        if (!from_b) return 0;
        if (!refl_comp_field_nametype(f, from_b)) return 0;
    }

    return 1;
}

int refl_comp_struct_schema(RStruct* generated,  RStruct* model);

static int refl_comp_field_schema(RField* g, RField* m) {
    if (m->kind == K_OPTION) {
        if (!g) return 1;
        if (g->kind == K_OPTION) return 0;
        return refl_comp_field_schema(g, m->subtype.subfield);
    }
    if ((!m)^(!g)) return 0;
    if (!m) return 1; 
    if (m->kind != g->kind) return 0;
    switch (m->kind)
    {
    case K_PRIMITIVE:
        if (g->subtype.prim == P_U0) return 1;
        return refl_primitive_can_promote(g->subtype.prim, m->subtype.prim);
    case K_STRUCT:
        return refl_comp_struct_schema(g->subtype.s, m->subtype.s);
    case K_UNION:
        return strcmp(m->subtype.u->name_type, g->subtype.u->name_type) == 0;
    case K_ENUM:
        return strcmp(m->subtype.e->name_type, g->subtype.e->name_type) == 0;
    case K_FIXED_ARRAY:
    case K_DYN_ARRAY:
    case K_POINTER:
        if (m->size < g->size) return 0;
        return refl_comp_field_schema(g->subtype.subfield, m->subtype.subfield);    
    default:
        break;
    }
    return 1;
}

int refl_comp_struct_schema(RStruct* generated, RStruct* model) {
    if (!generated ^ !model) return 0;
    if (!generated) return 1;
    for (size_t i = 0; i < model->n_elements; i++)
    {
        RField* m = model->fields[i];
        RField* g = refl_field_by_name(generated, m->name, NULL);
        if (!refl_comp_field_schema(g, m)) { 
            return 0; 
        }
    }
    return 1;
}

int refl_comp_struct_copy(RStruct* original, RStruct* new) {
    if (original->n_elements > new->n_elements) return 0;
    const char* first_name = original->fields[0]->name;
    size_t index = (size_t)-1;
    for (size_t i = 0; i < new->n_elements; i++)
    {
        if (!strcmp(new->fields[i]->name, first_name))
            index = i;
    }
    if (index == (size_t)-1) return 0;
    if (new->fields[index]->kind != original->fields[0]->kind)
        return 0;

    size_t initial_offset = new->fields[index]->offset;

    for (size_t i = 1; i < original->n_elements; i++)
    {
        RField* og = original->fields[i];
        RField* n = new->fields[index+i];
        if (
            strcmp(n->name, og->name)
            || (n->kind != og->kind)
            || (n->offset - initial_offset != og->offset))
            return 0;
    }
    
    return 1;
}


int refl_enum_migrate_by_label(REnum* old, REnum* new, int val) {
    const char* name = refl_enum_get_label(old, val);
    if (!name) return -1;
    for (size_t i = 0; i < new->n_elements; i++)
    {
        if (strcmp(new->names[i], name) == 0) {
            return new->indexes[i];
        }
    }
    return -1;
}

static int refl_primitive_can_promote(RPrimitive a, RPrimitive b) {
    static const uint8_t sizes[] = {
        [P_U0]=0,
        [P_U8]=1,[P_I8]=1,
        [P_U16]=2,[P_I16]=2,
        [P_U32]=4,[P_I32]=4,
        [P_U64]=8,[P_I64]=8,
        [P_F32]=4,[P_F64]=8,
        [P_BOOL]=1
    };

    if (a == b) return 1;

    // int -> larger int
    if (a <= P_I64 && b <= P_I64)
        return sizes[a] <= sizes[b];

    // int -> float
    if (a <= P_I64 && (b == P_F32 || b == P_F64))
        return 1;

    // float -> larger float
    if ((a == P_F32 && b == P_F64))
        return 1;

    return 0;
}

static void refl_primitive_convert(
    RPrimitive from,
    RPrimitive to,
    const void* src,
    void* dst
) {
    double f = 0.0;
    int64_t i = 0;
    int is_float = 0;

    /* ---- LOAD ---- */
    switch (from) {
        case P_F32: f = *(const float*)src; is_float = 1; break;
        case P_F64: f = *(const double*)src; is_float = 1; break;

        case P_I8:  i = *(const int8_t*)src; break;
        case P_U8:  i = *(const uint8_t*)src; break;
        case P_I16: i = *(const int16_t*)src; break;
        case P_U16: i = *(const uint16_t*)src; break;
        case P_I32: i = *(const int32_t*)src; break;
        case P_U32: i = *(const uint32_t*)src; break;
        case P_I64: i = *(const int64_t*)src; break;
        case P_U64: i = *(const uint64_t*)src; break;
        case P_BOOL:i = *(const _Bool*)src; break;
        default: return;
    }

    /* ---- STORE ---- */
    switch (to) {
        case P_F32:
            *(float*)dst = is_float ? (float)f : (float)i;
            break;
        case P_F64:
            *(double*)dst = is_float ? f : (double)i;
            break;

        case P_I8:  *(int8_t*)dst  = (int8_t)(is_float ? f : i); break;
        case P_U8:  *(uint8_t*)dst = (uint8_t)(is_float ? f : i); break;
        case P_I16: *(int16_t*)dst = (int16_t)(is_float ? f : i); break;
        case P_U16: *(uint16_t*)dst= (uint16_t)(is_float ? f : i); break;
        case P_I32: *(int32_t*)dst = (int32_t)(is_float ? f : i); break;
        case P_U32: *(uint32_t*)dst= (uint32_t)(is_float ? f : i); break;
        case P_I64: *(int64_t*)dst = (int64_t)(is_float ? f : i); break;
        case P_U64: *(uint64_t*)dst= (uint64_t)(is_float ? f : i); break;

        case P_BOOL:
            *(int*)dst = (is_float ? f != 0.0 : i != 0);
            break;

        default:
            break;
    }
}


static void migrate_array(
    RField* f_old,
    RField* f_new,
    void* src,
    void* dst,
    size_t len
) {
    
    size_t old_len = len;
    size_t new_len = len;

    RField* e_old = f_old->subtype.subfield;
    RField* e_new = f_new->subtype.subfield;

    if (e_old->kind == K_PRIMITIVE && e_old->subtype.prim == P_U0) return;

    size_t count = old_len < new_len ? old_len : new_len;

    size_t old_stride = refl_get_field_size(e_old);
    size_t new_stride = refl_get_field_size(e_new);

    /* dst is already zeroed by caller */

    for (size_t i = 0; i < count; i++) {
        void* src_elem = (uint8_t*)src + i * old_stride;
        void* dst_elem = (uint8_t*)dst + i * new_stride;

        /* Same kind -> recurse */
        if (e_old->kind == e_new->kind) {
            switch (e_old->kind) {
            case K_PRIMITIVE:
                if (refl_primitive_can_promote(
                        e_old->subtype.prim,
                        e_new->subtype.prim)) {
                    refl_primitive_convert(
                        e_old->subtype.prim,
                        e_new->subtype.prim,
                        src_elem,
                        dst_elem
                    );
                }
                break;

            case K_STRUCT: {
                void* nested = refl_struct_migrate(
                    e_old->subtype.s,
                    e_new->subtype.s,
                    src_elem
                );
                if (nested) {
                    memcpy(dst_elem, nested, e_new->subtype.s->total_size);
                    free(nested);
                }
                break;
            }

            default:
                /* same kind, same size assumption */
                memcpy(dst_elem, src_elem,
                       old_stride < new_stride ? old_stride : new_stride);
                break;
            }
        }

        /* Different kinds but compatible primitive promotion */
        else if (e_old->kind == K_PRIMITIVE &&
                 e_new->kind == K_PRIMITIVE &&
                 refl_primitive_can_promote(
                     e_old->subtype.prim,
                     e_new->subtype.prim)) {
            refl_primitive_convert(
                e_old->subtype.prim,
                e_new->subtype.prim,
                src_elem,
                dst_elem
            );
        }

        /* Otherwise: incompatible element -> skip */
    }
}


static void refl_migrate_field(
    RField* f_old,
    RField* f_new,
    uint8_t* src,
    uint8_t* dst,
    int wrapped_src,
    int wrapped_dst
) {
    int val = *(int*)src;

    if (f_old->kind != f_new->kind) goto diferent;

    switch (f_old->kind)
    {
    case K_PRIMITIVE:
        if (refl_primitive_can_promote(f_old->subtype.prim, f_new->subtype.prim)) {
            refl_primitive_convert(
                f_old->subtype.prim,
                f_new->subtype.prim,
                src, dst
            );
        }
        return;

    case K_STRING:
        *(char**)dst = strdup(*(char**)src);
        return;

    case K_ENUM:
        *(int*)dst = refl_enum_migrate_by_label(
            f_old->subtype.e,
            f_new->subtype.e,
            val
        );
        return;

    case K_STRUCT: {
        void* _src = src;
        void* _dst = dst;
        if (wrapped_dst) {
            *(void**)dst = malloc(f_new->subtype.s->total_size);
            _dst = *(void**)dst;
        } else if (wrapped_src){
            _src = *(void**)src;
        }

        void* nested = refl_struct_migrate(
            f_old->subtype.s,
            f_new->subtype.s,
            _src
        );
        if (nested) {
            memcpy(_dst, nested, f_new->subtype.s->total_size);
            free(nested);
        }
        return;
    }

    case K_FIXED_ARRAY:
        migrate_array(f_old, f_new, src, dst, f_old->size);
        return;

    case K_DYN_ARRAY:
        r_array_t* as = (r_array_t*)src;
        r_array_t* ad = (r_array_t*)dst;
        ad->size = as->size;
        ad->capacity = as->capacity;
        ad->chunk_size = refl_get_field_size(f_new->subtype.subfield);
        ad->array = calloc(ad->chunk_size, ad->size);
        migrate_array(f_old, f_new, as->array, ad->array, as->size);
        return;

    case K_POINTER:
        memcpy(dst, src, sizeof(void*));
        return;

    case K_OPTION:  
        r_option_t* o = (r_option_t*)src;
        r_option_t* n = (r_option_t*)dst;

        n->opt = o->opt;
        if (o->opt == SOME) {
            refl_migrate_field(
                f_old->subtype.subfield,
                f_new->subtype.subfield,
                (uint8_t*)&o->optional,
                (uint8_t*)&n->optional,
                1,1
            );
        }
        return;

    default:
        return;
    }

diferent:

    if (f_old->kind == K_OPTION && f_new->kind != K_OPTION) {
        r_option_t* o = (r_option_t*)src;
        if (o->opt == SOME) {
            refl_migrate_field(
                f_old->subtype.subfield,
                f_new,
                (uint8_t*)&o->optional,
                dst,
                1, 0
            );
        }
    }

    else if (f_old->kind != K_OPTION && f_new->kind == K_OPTION) {
        r_option_t* o = (r_option_t*)dst;
        o->opt = SOME;
        o->size = f_old->next_offset - f_old->offset;

        refl_migrate_field(
            f_old,
            f_new->subtype.subfield,
            src,
            (void*)&o->optional,
            0, 1
        );

        o->size = f_new->subtype.subfield->next_offset -
                  f_new->subtype.subfield->offset;
    }

    else if (f_old->kind == K_FIXED_ARRAY && f_new->kind == K_DYN_ARRAY) {
        size_t old_sz = refl_get_field_size(f_old);
        size_t new_sz = refl_get_field_size(f_new->subtype.subfield) * f_old->size;
        if (new_sz < old_sz) return;
        r_array_t* arr = (r_array_t*)dst;
        arr->chunk_size = new_sz / f_old->size;
        arr->capacity = arr->size = new_sz / arr->chunk_size;
        arr->array = calloc(arr->chunk_size, arr->size);
        if (old_sz == new_sz) {
            memcpy(arr->array, src, arr->capacity * arr->chunk_size);
        } else {
            migrate_array(f_old, f_new, src, arr->array, f_old->size);
        }
    }

    else if (f_old->kind == K_DYN_ARRAY && f_new->kind == K_FIXED_ARRAY) {
        r_array_t* arr = (r_array_t*)src;
        size_t new_size = refl_get_field_size(f_new->subtype.subfield);
        if (arr->chunk_size > new_size) return;
        migrate_array(f_old, f_new, arr->array, dst, f_new->size);
    }
}


/**
 * Migration by name and compatible type.
 * Doesn't check compatibility; best-effort.
 */
RObject refl_struct_migrate(RStruct* old, RStruct* new, RObject instance) {
    if (!old || !new || !instance) return NULL;

    uint8_t* out = calloc(1, new->total_size);
    memset(out, 0, new->total_size);
    if (!out) return NULL;

    for (size_t i = 0; i < old->n_elements; i++) {
        RField* f_old = old->fields[i];
        RField* f_new = refl_field_by_name(new, f_old->name, NULL);
        if (!f_new) continue;

        uint8_t* src = (uint8_t*)instance + f_old->offset;
        uint8_t* dst = out + f_new->offset;

        refl_migrate_field(f_old, f_new, src, dst, 0, 0);
    }

    return out;
}
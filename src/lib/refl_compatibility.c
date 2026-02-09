#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <public/core.h>
#include <public/types.h>
#include <public/cmp.h>

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
            if (!refl_comp_struct_nametype_subset(a->subtype.s, b->subtype.s)) return 0;
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

int refl_comp_struct_nametype_subset(RStruct* tiny, RStruct* large) {
    if (!large^!tiny) return 0;
    if (!tiny) return 1;
    if (strcmp(tiny->name_type, large->name_type) == 0) return 1;
    for (size_t i = 0; i < tiny->n_elements; i++)
    {
        RField* f = tiny->fields[i];
        if (f->kind == K_OPTION) continue;
        RField* from_b = refl_field_by_name(large, f->name);
        if (!from_b) return 0;
        if (!refl_comp_field_nametype(f, from_b)) return 0;
    }

    return 1;
}

int refl_comp_struct_schema(RStruct* model, RStruct* generated);

static int refl_comp_field_schema(RField* m, RField* g) {
    if (m->kind == K_OPTION) {
        if (!g) return 1;
        if (g->kind == K_OPTION) return 0;
        return refl_comp_field_schema(m->subtype.subfield, g);
    }
    if ((!m)^(!g)) return 0;
    if (!m) return 1; 
    if (m->kind != g->kind) return 0;
    switch (m->kind)
    {
    case K_PRIMITIVE:
        return refl_primitive_can_promote(g->subtype.prim, m->subtype.prim);
    case K_STRUCT:
        return refl_comp_struct_schema(m->subtype.s, g->subtype.s);
    case K_UNION:
        return strcmp(m->subtype.u->name_type, g->subtype.u->name_type) == 0;
    case K_ENUM:
        return strcmp(m->subtype.e->name_type, g->subtype.e->name_type) == 0;
    case K_FIXED_ARRAY:
    case K_DYN_ARRAY:
    case K_POINTER:
        if (m->size < g->size) return 0;
        return refl_comp_field_schema(m->subtype.subfield, g->subtype.subfield);    
    default:
        break;
    }
    return 1;
}

int refl_comp_struct_schema(RStruct* model, RStruct* generated) {
    for (size_t i = 0; i < model->n_elements; i++)
    {
        RField* m = model->fields[i];
        RField* g = refl_field_by_name(generated, m->name);
        if (!refl_comp_field_schema(m,g)) return 0;
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
        case P_BOOL:i = *(const int*)src; break;
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


static void migrate_fixed_array(RField* f_old, RField* f_new, void* src, void* dst) {
    if (f_old->size > f_new->size) return;
    if (f_old->subtype.subfield->kind != f_new->subtype.subfield->kind)
        return;

    if (f_old->subtype.subfield->kind == K_PRIMITIVE &&
        refl_primitive_can_promote(
                f_old->subtype.subfield->subtype.prim,
                f_new->subtype.subfield->subtype.prim)) {
        for (size_t i = 0; i < f_old->size; i++)
        {
            refl_primitive_convert(
                f_old->subtype.subfield->subtype.prim,
                f_new->subtype.subfield->subtype.prim,
                (uint8_t*)src + (refl_prim_to_size(f_old->subtype.subfield->subtype.prim) * i),
                (uint8_t*)dst + (refl_prim_to_size(f_new->subtype.subfield->subtype.prim) * i)
            );
        }
        return;
        
    }
    memcpy(dst, src, f_old->next_offset - f_old->offset);
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
        RField* f_new = refl_field_by_name(new, f_old->name);
        if (!f_new) continue;

        uint8_t* src = (uint8_t*)instance + f_old->offset;
        uint8_t* dst = out + f_new->offset;

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
            continue;

        case K_STRING:
            *(char**)dst = *(char**)src;
            continue;
        case K_ENUM:
            *(int*)dst = refl_enum_migrate_by_label(f_old->subtype.e, f_new->subtype.e, val);
            continue;
        case K_STRUCT:
            void* nested = refl_struct_migrate(
                f_old->subtype.s,
                f_new->subtype.s,
                src
            );
            if (nested) {
                memcpy(dst, nested, f_new->subtype.s->total_size);
                free(nested);
            }
            continue;

        case K_FIXED_ARRAY:
            migrate_fixed_array(f_old, f_new ,src, dst);
            continue;

        case K_DYN_ARRAY:
            if (f_old->subtype.subfield->kind == f_new->subtype.subfield->kind)
                memcpy(dst, src, sizeof(r_array_t));
            continue;

        case K_POINTER:
            memcpy(dst, src, sizeof(void*));
            break;
        default:
            continue;
        }
diferent:

        /* ---- OPTION ---- */
        if (f_old->kind == K_OPTION && f_new->kind == K_OPTION) {
            r_option_t* o = (r_option_t*)src;
            r_option_t* n = (r_option_t*)dst;
            n->opt = o->opt;
            if (o->opt == SOME) {
                n->optional = o->optional;
                n->size = o->size;
            }
        }

        else if (f_old->kind == K_OPTION && f_new->kind != K_OPTION) {
            if (f_old->subtype.subfield->kind != f_new->kind) continue;
            r_option_t* o = (r_option_t*)src;
            if (o->opt == SOME) {
                memcpy(dst, o->optional, f_new->next_offset - f_new->offset);
            }
        }

        else if (f_old->kind != K_OPTION && f_new->kind == K_OPTION) {
            r_option_t* o = (r_option_t*)dst;
            o->opt = SOME;
            if (f_old->kind == K_STRUCT) {
                o->optional = malloc(f_old->subtype.s->total_size);
                memcpy(o->optional, src, f_old->subtype.s->total_size);
            
            } else {
                o->optional = *(void**)src;
            }
            o->size = f_old->offset - f_old->next_offset;
        }

        else if (f_old->kind == K_FIXED_ARRAY && f_new->kind == K_DYN_ARRAY) {
/*             uint8_t* next = out + f_old->offset; */
            size_t sz = f_old->next_offset - f_old->offset;
            r_array_t* arr = (r_array_t*)dst;
            arr->chunk_size = sz / f_old->size;
            arr->capacity = arr->size = sz / arr->chunk_size;
            arr->array = calloc(arr->chunk_size, arr->size);
            memcpy(arr->array, src, arr->capacity * arr->chunk_size);
        }
    }

    return out;
}

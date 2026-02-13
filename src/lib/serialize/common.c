#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <crefl/types.h>
#include <crefl/core.h>

#include <serial/_common.h>

#define BUFFERS_INITIAL 64

void generic_numerical_converter(RPrimitive p, uint64_t bytes, void* buff, size_t sz) {
    switch (p)
    {
    case P_BOOL:
        bytes &= 0xFF;
        (bytes >= 1)
        ? snprintf(buff, sz, "true")
        : snprintf(buff, sz, "false");
        break;
    case P_U0:
        snprintf(buff, sz, "<no_val>");
        break;
    case P_U8:
        snprintf(buff, sz, "%hhu", (uint8_t)(bytes & 0xFF));
        break;
    case P_I8:
        snprintf(buff, sz, "%hhd", (int8_t)(bytes));
        break;
    case P_U16:
        snprintf(buff, sz, "%hu", (uint16_t)(bytes & 0xFFFF));
        break;
    case P_I16:
        snprintf(buff, sz, "%hd", (int16_t)(bytes));
        break;
    case P_U32:
        snprintf(buff, sz, "%u", (uint32_t)(bytes & 0xFFFFFFFF));
        break;
    case P_I32:
        snprintf(buff, sz, "%d", (int32_t)(bytes));
        break;
    case P_U64:
        snprintf(buff, sz, "%lu", (bytes));
        break;
    case P_I64:
        snprintf(buff, sz, "%ld", (int64_t)bytes);
        break;
    case P_F32:
        union { uint32_t u; float f; } v = { .u = (uint32_t)bytes };
        snprintf(buff, sz, "%f", v.f);
        break;
    case P_F64:
        union { uint64_t u; double d; } v2 = { .u = bytes };
        snprintf(buff, sz, "%lf", v2.d);
        break;
    default:
        break;
    }
}

typedef struct {
    char* buff;
    size_t size;
    size_t cap;
} char_vec;

static void str_push(char_vec* cv, const char* str) {
    if (str == NULL) {
        str_push(cv, ("(null)"));
        return;
    }
    size_t sz = strlen(str);

    while (cv->size + sz + 1 > cv->cap) {
        size_t old = cv->cap;
        cv->cap *= 2;
        cv->buff = realloc(cv->buff, cv->cap);
        memset(cv->buff + old, 0, cv->cap - old);
    }

    memcpy(cv->buff + cv->size, str, sz);
    cv->size += sz;
    cv->buff[cv->size] = '\0';
}


static void str_push_tabs(SerializePolicy* sp, char_vec* cv, int amnt) {
    for (int i = 0; i < amnt; i++)
    {
        str_push(cv, sp->indent_str);
    }
}

static void load_numeric_bytes(RField* f, void* instance, uint64_t* out) {
    *out = 0UL;
    memcpy(out, (uint8_t*)instance + f->offset, refl_prim_to_size(f->subtype.prim));
}



static void gc_struct(SerializePolicy* sp, RStruct* s, void* instance, int indent, char_vec* v, char* buff2);


static void gc_field(SerializePolicy* sp, RField* f, void* instance, int indent, char_vec* v, char* buff2) {
    r_option_t* o;
    r_array_t* a;
    
    switch (f->kind)
        {
        case K_PRIMITIVE:
            uint64_t raw;
            load_numeric_bytes(f, instance, &raw);
            sp->format_primitive(f->subtype.prim, raw, buff2, 50);
            str_push(v, buff2);
            break;

        case K_STRING:
            char* str = *(char**)((uint8_t*)instance + f->offset);
            if (str) str_push(v, sp->str_start);
            str_push(v, str);
            if (str) str_push(v, sp->str_end);
            break;
        case K_OPTION:
            o = (r_option_t*)((uint8_t*)instance + f->offset);
            if (sp->unwrap_optionals && o->opt == NONE) return;
            if (o->opt == SOME) {
                if (!sp->unwrap_optionals)str_push(v, sp->opt_some_start);
                if (f->subtype.subfield->kind == K_STRUCT || 
                    f->subtype.subfield->kind == K_UNION) {
                    gc_field(sp, f->subtype.subfield, o->optional, indent, v, buff2);
                } else {
                    gc_field(sp, f->subtype.subfield, (void*)&o->optional, indent, v, buff2);
                }
                if (!sp->unwrap_optionals)str_push(v, sp->opt_some_end);
            } else {
                str_push(v, sp->opt_none);
            }break;
        case K_DYN_ARRAY:
            a = (r_array_t*)((uint8_t*)instance + f->offset);
            str_push(v, sp->arr_start);
            for (size_t i = 0; i < a->size; i++)
            {
                gc_field(sp, f->subtype.subfield, (void*)((uint8_t*)a->array + (i * a->chunk_size)), indent, v, buff2);
                if (i != a->size - 1) str_push(v, sp->arr_sep);
            }
            str_push(v, sp->arr_end);break;
        case K_FIXED_ARRAY:
            str_push(v, sp->arr_start);

            for (size_t i = 0; i < f->size; i++)
            {
                gc_field(sp, f->subtype.subfield, (uint8_t*)instance + f->offset + (i * refl_get_field_size(f->subtype.subfield)), indent, v, buff2);
                if (i != f->size - 1) str_push(v, sp->arr_sep);
            }
            str_push(v, sp->arr_end);break;
        case K_ENUM:
            if (sp->enum_as_string) {
                int val;
                memcpy(&val, (uint8_t*)instance + f->offset, sizeof(int));
                str_push(v, (char*)refl_enum_get_label(f->subtype.e, val));
            } else {
                uint64_t raw;
                load_numeric_bytes(f, instance, &raw);
                sp->format_primitive(f->subtype.prim, raw, buff2, 50);
                str_push(v, buff2);
            }
            break;
        case K_STRUCT:
            gc_struct(sp, f->subtype.s, (uint8_t*)instance + f->offset, indent, v, buff2);break;
        case K_POINTER:
            void* ptr = *(void**)((uint8_t*)instance + f->offset);
            snprintf(buff2, 50, "%p", ptr);
            str_push(v, buff2);
            break;
        case K_UNION:
            str_push(v, "<union>");break;
        default:
            break;
        }
        

}

static void gc_struct(SerializePolicy* sp, RStruct* s, void* instance, int indent, char_vec* v, char* buff2) {
    str_push(v, sp->obj_start);
    if (sp->pretty) str_push(v, sp->newline);
    indent++;
    int skip;
    for (size_t i = 0; i < s->n_elements; i++)
    {
        skip = 0;
        
        RField* f = s->fields[i];
        // Lookahead for NONE + unwrap_optionals
        if (sp->unwrap_optionals) {
            r_option_t* o = (r_option_t*)((uint8_t*)instance + f->offset);
            if (o->opt == NONE) skip = 1;
        }
        if (!skip) {
            if (sp->pretty) str_push_tabs(sp, v,indent);
            if (sp->scape_keys)str_push(v, sp->key_start);
            str_push(v, (char*)f->name);
            if (sp->scape_keys) str_push(v, sp->key_end);
            str_push(v, sp->key_val_sep);
            gc_field(sp, f, instance, indent, v, buff2); 
            if (i != s->n_elements - 1) {
                str_push(v, sp->field_sep);
                if (sp->pretty) str_push(v, sp->newline);
            }
        }
    }
    indent--;
    if (sp->pretty) { 
        str_push(v, sp->newline);
        str_push_tabs(sp, v,indent);
    }
    str_push(v, sp->obj_end);
}

void* walk_type(SerializePolicy* sp, RStruct* s, void* instance) {
    if (!s) return NULL;
    char_vec v = { .buff = malloc(BUFFERS_INITIAL), .cap = BUFFERS_INITIAL, .size = 0 };
    memset(v.buff, 0, BUFFERS_INITIAL);
    char buff[BUFFERS_INITIAL];
    if (sp->format_primitive == NULL) sp->format_primitive = generic_numerical_converter;
    gc_struct(sp, s, instance, 0, &v, buff);
    return v.buff;
}


// ================== COLLECTIONS ============================= //
temp_field_pool _fp_init(size_t initial) {
    temp_field_pool fp = {0};
    fp.f = calloc(sizeof(RField), initial);
    fp.capacity = initial;
    fp.sz = 0;
    return fp;
}

void _fp_append(temp_field_pool* fp, RField* f) {
    if (fp->sz >= fp->capacity) {
        fp->f = realloc(fp->f, sizeof(RField) * 2 * fp->capacity);
        fp->capacity *= 2;
    }
    memcpy(&fp->f[fp->sz++], f, sizeof(RField));
}

void _fp_free(temp_field_pool* fp) { free(fp->f); }


g_arraylist_t _gal_init(size_t initial, size_t chunk_size) {
    g_arraylist_t gal = {0};
    gal.capacity = initial;
    gal.chunk_size = chunk_size;
    gal.data = calloc(chunk_size, initial);
    gal.size = 0;
    return gal;
}

void _gal_append(g_arraylist_t* gal, void* object) {
    if (gal->size >= gal->capacity) {
        gal->data = realloc(gal->data, gal->capacity * 2 * gal->chunk_size);
        gal->capacity *= 2;
    }
    memcpy(&((uint8_t*)gal->data)[gal->size++ * gal->chunk_size], object, gal->chunk_size);
}

void _gal_free(g_arraylist_t* gal) { free(gal->data); }

unordered_buffer _uob_init(size_t initial) {
    unordered_buffer uob = {0};
    uob.data = calloc(1, initial);
    uob.cur = 0;
    uob.capacity = initial;
    return uob;
}

void _uob_append(unordered_buffer* uob, void* src, size_t sz) {
    if (uob->cur + sz >= uob->capacity) {
        uob->data = realloc(uob->data, 2 * uob->capacity);
        uob->capacity *= 2;
    }
    memcpy(uob->data + uob->cur, src, sz);
    uob->cur += sz;
}

void _uob_free(unordered_buffer* uob) { free(uob->data); }
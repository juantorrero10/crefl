#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <stdint.h>

#include <crefl/types.h>
#include <crefl/core.h>

#include <crefl/serial/_common.h>
#include <_arena.h>
#define BUFFERS_INITIAL 64

void generic_numeric_to_str(RPrimitive p, u64 bytes, void* buff, size_t sz) {
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
        snprintf(buff, sz, "%hhu", (u8)(bytes & 0xFF));
        break;
    case P_I8:
        snprintf(buff, sz, "%hhd", (i8)(bytes));
        break;
    case P_U16:
        snprintf(buff, sz, "%hu", (u16)(bytes & 0xFFFF));
        break;
    case P_I16:
        snprintf(buff, sz, "%hd", (i16)(bytes));
        break;
    case P_U32:
        snprintf(buff, sz, "%u", (u32)(bytes & 0xFFFFFFFF));
        break;
    case P_I32:
        snprintf(buff, sz, "%d", (i32)(bytes));
        break;
    case P_U64:
        snprintf(buff, sz, "%lu", (bytes));
        break;
    case P_I64:
        snprintf(buff, sz, "%ld", (i64)bytes);
        break;
    case P_F32:
        union { u32 u; float f; } v = { .u = (u32)bytes };
        snprintf(buff, sz, "%f", v.f);
        break;
    case P_F64:
        union { u64 u; double d; } v2 = { .u = bytes };
        snprintf(buff, sz, "%lf", v2.d);
        break;
    default:
        break;
    }
}


static inline RPrimitive _smallest_primitive_type_unsigned(u64 v, u8* s) {
    *s = 0;
    if (v <= UINT8_MAX)  {*s = 1;return P_U8;}
    if (v <= UINT16_MAX) {*s = 2;return P_U16;}
    if (v <= UINT32_MAX) {*s = 4;return P_U32;}
    if (v <= UINT64_MAX) {*s = 8;return P_U64;}
    return P_U0;
}

static inline RPrimitive _smallest_primitive_type_signed(i64 v, u8* s) {
    *s = 0;
    if (v <= INT8_MAX && v >= INT8_MIN)  {*s = 1;return P_I8;}
    if (v <= INT16_MAX && v >= INT16_MIN) {*s = 2;return P_I16;}
    if (v <= INT32_MAX && v >= INT32_MIN) {*s = 4;return P_I32;}
    if (v <= INT64_MAX && v >= INT64_MIN) {*s = 8;return P_I64;}
    return P_U0;
}

int generic_str_to_numeric(
    stream_walker* sw,
    u64* out_bytes,
    RPrimitive* out_prim_kind
) {
    size_t start= sw->cur;
    size_t cur = sw->cur;
    size_t eof = sw->eof;
    u8* buff = sw->buff;
    u8 is_float=0;
    u8 sign_bit=0;

    // Strategy: look head to figure out if its int or float
    if (cur < eof) {
        if (buff[cur] == '+') cur++;
        else if (buff[cur]=='-'){sign_bit=1;cur++;} 
    }

    while(cur < eof && isdigit(buff[cur])) cur++;

    // set to float and skip until digits end
    if (cur < eof && buff[cur] == '.') {
        is_float = 1;
        cur++;
        while(cur < eof && isdigit(buff[cur])) cur++;
    }

    // Handle exponent
    if (cur < eof && (
        buff[cur] == 'e' || buff[cur] == 'E'
    )) {
        is_float = 1;
        cur++;
        while(cur < eof && isdigit(buff[cur])) cur++;
    }

    // cur should point at the end of the ASCII representation by now.
    sw->cur = cur;
    char* extracted = arena_alloc(0, cur - start + 1);
    memcpy(extracted, buff + start, cur - start);
    u8 s;
    errno = 0;

    if (is_float) {
        double d = strtod((const char*)extracted, NULL);
        if (errno) return 0;
        float f = (float)d;
        // Return P_F32 if lossless
        if (f == d) {
            *out_prim_kind = P_F32;
            memset(out_bytes, 0, sizeof(u64));
            memcpy(out_bytes, &f, sizeof(float));
        } else {
            *out_prim_kind = P_F64;
            memset(out_bytes, 0, sizeof(u64));
            memcpy(out_bytes, &d, sizeof(double));
        }
    } else if (sign_bit) {
        i64 v = strtol((char*)extracted, NULL, 10);
        if (errno) return 0;
        *out_prim_kind = _smallest_primitive_type_signed(v, &s);
        memset(out_bytes, 0, sizeof(u64));
        memcpy(out_bytes, &v, s);
    } else {
        u64 v = strtoul((char*)extracted, NULL, 10);
        if (errno) return 0;
        *out_prim_kind = _smallest_primitive_type_unsigned(v, &s);
        memset(out_bytes, 0, sizeof(u64));
        memcpy(out_bytes, &v, s);
    }
    arena_dealloc(0, cur - start + 1);
    return 1;
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
        arena_alloc(0, cv->cap);
        cv->cap *= 2;
    }

    memcpy(cv->buff + cv->size, str, sz);
    cv->size += sz;
    cv->buff[cv->size] = '\0';
}

static void str_push_sized(char_vec* cv, const char* str, size_t sz) {
    if (str == NULL) {
        str_push(cv, ("(null)"));
        return;
    }

    while (cv->size + sz + 1 > cv->cap) {
        arena_alloc(0, cv->cap);
        cv->cap *= 2;
    }

    memcpy(cv->buff + cv->size, str, sz);
    cv->size += sz;
    cv->buff[cv->size] = '\0';
}

static void char_push(char_vec* cv, int c) {

    while (cv->size + 1 > cv->cap) {
        arena_alloc(0, cv->cap);
        cv->cap *= 2;
    }
    cv->buff[cv->size++] = (char)c;
    cv->buff[cv->size] = '\0';
}




static void str_push_tabs(SerializePolicy* sp, char_vec* cv, int amnt) {
    for (int i = 0; i < amnt; i++)
    {
        str_push(cv, sp->indent_str);
    }
}

static const char* _escape_lut[256] = {
    ['\\'] = "\\\\", ['\n'] = "\\n", ['\t'] = "\\t",
    ['\b'] = "\\b",  ['\r'] = "\\r", ['\f'] = "\\f", ['\"'] = "\\\""
};

const char* scape_search = "\\\n\t\b\r\f\"";

#define MEMCPY_MIN 16

static void scape_string(char_vec* cv, char* str) {
    char* curr = str;
    char* next_esc;
    char* escaped;
    size_t chunk;
    while((next_esc = strpbrk(curr, scape_search)) != NULL) {
        
        chunk = (size_t)(next_esc - curr);
        if (chunk == 0) goto __skip_copying;
        if (chunk > MEMCPY_MIN) {
            str_push_sized(cv, curr, chunk);
            curr += chunk;
        } else {
            while(chunk) {char_push(cv, *(curr++)); chunk--;}
        }
__skip_copying:
        escaped = (char*)_escape_lut[(u32)(*(curr++))];
        str_push_sized(cv, escaped, 2);
    }
    // copy the tail
    if (*curr) {
        str_push(cv, curr);
    }
}

static void load_numeric_bytes(RField* f, void* instance, u64* out) {
    *out = 0UL;
    memcpy(out, (u8*)instance + f->offset, refl_prim_to_size(f->subtype.prim));
}



static void gc_struct(SerializePolicy* sp, RStruct* s, void* instance, int indent, char_vec* v, char* buff2);


static void gc_field(SerializePolicy* sp, RField* f, void* instance, int indent, char_vec* v, char* buff2) {
    r_array_t* a;
    
    switch (f->kind)
        {
        case K_PRIMITIVE:
            u64 raw;
            load_numeric_bytes(f, instance, &raw);
            sp->format_primitive(f->subtype.prim, raw, buff2, 50);
            str_push(v, buff2);
            break;

        case K_STRING:
            char* str = *(char**)((u8*)instance + f->offset);
            if (str) str_push(v, sp->str_start);

            if (str == NULL) {
                if (sp->null_str) str_push(v, sp->null_str);
                else str_push(v, NULL);
                break;
            }

            if (sp->scape_special_ascii) {
                scape_string(v, str);
            } else {
                str_push(v, str);
            }

            if (str) str_push(v, sp->str_end);
            break;
        case K_OPTION:
            {
                void* option_value = (u8*)instance + f->offset;
                if (sp->unwrap_optionals && !refl_option_is_some(option_value)) return;
                if (refl_option_is_some(option_value)) {
                    if (!sp->unwrap_optionals)str_push(v, sp->opt_some_start);
                    gc_field(sp, f->subtype.subfield, option_value, indent, v, buff2);
                    if (!sp->unwrap_optionals)str_push(v, sp->opt_some_end);
                } else {
                    str_push(v, sp->opt_none);
                }
            }break;
        case K_DYN_ARRAY:
            a = (r_array_t*)((u8*)instance + f->offset);
            str_push(v, sp->arr_start);
            for (size_t i = 0; i < a->size; i++)
            {
                gc_field(sp, f->subtype.subfield, (void*)((u8*)a->array + (i * a->chunk_size)), indent, v, buff2);
                if (i != a->size - 1) str_push(v, sp->arr_sep);
            }
            str_push(v, sp->arr_end);break;
        case K_FIXED_ARRAY:
            str_push(v, sp->arr_start);

            for (size_t i = 0; i < f->size; i++)
            {
                gc_field(sp, f->subtype.subfield, (u8*)instance + f->offset + (i * refl_get_field_size(f->subtype.subfield)), indent, v, buff2);
                if (i != f->size - 1) str_push(v, sp->arr_sep);
            }
            str_push(v, sp->arr_end);break;
        case K_ENUM:
            if (sp->enum_as_string) {
                int val;
                memcpy(&val, (u8*)instance + f->offset, sizeof(int));
                const char* lbl = refl_enum_get_label(f->subtype.e, val);
                str_push(v, sp->str_start);
                str_push(v, lbl);
                str_push(v, sp->str_end);
            } else {
                u64 raw;
                load_numeric_bytes(f, instance, &raw);
                sp->format_primitive(f->subtype.prim, raw, buff2, 50);
                str_push(v, buff2);
            }
            break;
        case K_STRUCT:
            gc_struct(sp, f->subtype.s, (u8*)instance + f->offset, indent, v, buff2);break;
        case K_POINTER:
            void* ptr = *(void**)((u8*)instance + f->offset);
            if (ptr == NULL && sp->null_ptr) {
                str_push(v, sp->null_ptr);
                break;            
            }
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
        if (sp->unwrap_optionals && f->kind == K_OPTION) {
            void* option_value = (u8*)instance + f->offset;
            if (!refl_option_is_some(option_value)) skip = 1;
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

size_t walk_type(SerializePolicy* sp, RStruct* s, void* instance, void** out) {
    if (!s) return 0;
    char_vec v = { .buff = arena_alloc(0, BUFFERS_INITIAL), .cap = BUFFERS_INITIAL, .size = 0};
    char buff[BUFFERS_INITIAL];
    if (sp->format_primitive == NULL) sp->format_primitive = generic_numeric_to_str;
    gc_struct(sp, s, instance, 0, &v, buff);
    arena_dealloc(0, v.cap);
    if (out) *out = strdup(v.buff); 
    return v.size;
}


// ================== COLLECTIONS ============================= //
temp_field_pool _fp_init(int n_arena, size_t initial) {
    temp_field_pool fp = {0};
    fp.f = arena_alloc(n_arena, sizeof(RField) * initial);
    fp.n_arena = n_arena;
    fp.capacity = initial;
    fp.sz = 0;
    return fp;
}

void _fp_append(temp_field_pool* fp, RField* f) {
    if (fp->sz >= fp->capacity) {
        arena_alloc(fp->n_arena, fp->capacity * sizeof(RField));
        fp->capacity *= 2;
    }
    memcpy(&fp->f[fp->sz++], f, sizeof(RField));
}

void _fp_free(temp_field_pool* fp) { 
    arena_dealloc(fp->n_arena, fp->capacity * sizeof(RField)); 
}


g_arraylist_t _gal_init(int n_arena, size_t initial, size_t chunk_size) {
    g_arraylist_t gal = {0};
    gal.capacity = initial;
    gal.chunk_size = chunk_size;
    gal.n_arena = n_arena;
    gal.data = arena_alloc(n_arena, initial * chunk_size);
    gal.size = 0;
    return gal;
}

void _gal_append(g_arraylist_t* gal, void* object) {
    if (gal->size >= gal->capacity) {
        arena_alloc(gal->n_arena, gal->chunk_size);
        gal->capacity += 1;
    }
    memcpy(&((u8*)gal->data)[gal->size++ * gal->chunk_size], object, gal->chunk_size);
}


void _gal_empty(g_arraylist_t* gal) { gal->size = 0; }

void _gal_free(g_arraylist_t* gal) { 
    arena_dealloc(gal->n_arena, gal->capacity); 
}

unordered_buffer _uob_init(int n_arena, size_t initial) {
    unordered_buffer uob = {0};
    uob.n_arena = n_arena;
    uob.data = arena_alloc(n_arena, initial);
    uob.cur = 0;
    uob.capacity = initial;
    return uob;
}

void _uob_expand(unordered_buffer* uob, size_t sz) {
    if (uob->cur + sz >= uob->capacity) {
        arena_alloc(uob->n_arena, sz);
        uob->capacity += sz;
    }
}

void _uob_append(unordered_buffer* uob, void* src, size_t sz) {
    _uob_expand(uob, sz);
    memcpy(uob->data + uob->cur, src, sz);
    uob->cur += sz;
}



void _uob_free(unordered_buffer* uob) { 
    arena_dealloc(uob->n_arena, uob->capacity); 
}
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#include <crefl/types.h>
#include <crefl/cmp.h>
#include <crefl/core.h>

#include <crefl/serial/err.h>
#include <crefl/serial/_common.h>
#include <_arena.h>

/*
    COMMENTED OUT TO make it compile: WIP
#define num_detect(_char) ({\
    int __ret = 2;\
    switch(_char) {\
        case '0':__ret=1;break;\
        case '1':__ret=1;break;\
        case '2':__ret=1;break;\
        case '3':__ret=1;break;\
        case '4':__ret=1;break;\
        case '5':__ret=1;break;\
        case '6':__ret=1;break;\
        case '7':__ret=1;break;\
        case '8':__ret=1;break;\
        case '9':__ret=1;break;\
        case '+':__ret=1;break;\
        case '-':__ret=1;break;\
        default:__ret=0;break;\
    }\
    __ret;\
})

#define key_valid(_char) ({\
    int __ret = 2;\
    if (isalnum(_char)) {\
        __ret = 1;\
    } else {\
        switch (_char)\
        {\
        case '_':__ret=1;break;\
        default:__ret=0;break;\
        }\
    }\
    __ret;\
})

#define wrapps_type(_field) (_field != K_OPTION) || (_field != K_DYN_ARRAY) || (_field != K_FIXED_ARRAY)

#define DYN_ARRAY_CAPACITY 16

#define currchar sw->buff[sw->cur]

static char s_cstr_buff[4096];

#define make_cstr(_str, size) ({            \
    size_t __Sz = (size);                   \
    memcpy(s_cstr_buff, (_str), __Sz);      \
    s_cstr_buff[__Sz] = '\0';               \
    s_cstr_buff;                            \
})

static inline RField* qp_search_field(RStruct* restrict m, const char* restrict field_name, size_t* f_idx) {
    for (size_t i = 0; i < m->n_elements; i++)
    {
        RField* f = m->fields[i];
        if (!strcmp(field_name, f->name)) { *f_idx = i; return f; }
    }
    return NULL;
}

// Advance cursor to the next key-value pair
// key1=value1&key2=value2
//      ^ START
//            ^ END
static DeserializeResult qp_read_val(
    RStruct* restrict s, 
    RField* restrict f, 
    stream_walker* restrict sw,
    size_t* restrict key_freqs, 
    u8* restrict write_to,
    bool wrapped_type
) {
    RField new_field = {0};
    bool bool_val;
    switch (currchar) {
        case 'f': {
            if (f->kind == K_PRIMITIVE && 
                f->subtype.prim == P_BOOL && 
                !memcmp("false", (const void*)&sw->buff[sw->cur], 5)
            ) {
                bool_val = false;
            }
            goto reading_bool;
        }
        case 't': {
            if (f->kind == K_PRIMITIVE && 
                f->subtype.prim == P_BOOL && 
                !memcmp("true", (const void*)&sw->buff[sw->cur], 4)
            ) {
                bool_val = true;
            }
            goto reading_bool;
        }
        default:
        if (num_detect(currchar) && f->kind == K_PRIMITIVE) {
            goto reading_num;
        }
        break;  
    }
reading_string:
    new_field.kind = K_STRING;
    new_field.size = 1;
    if (f->kind != K_ENUM && !refl_comp_field_schema(&new_field, f)) 
        return RET_ERR(DE_STREAM_INCOMPATIBLE);

    // Cheat a bit an write to arena and allocate later when we know the size
    char* alloced_dst = arena_alloc(0, 1);
    size_t string_sz;
    arena_dealloc(0, 1);
    while(sw->cur < sw->eof && currchar != '&') {
        *(alloced_dst + string_sz++) = currchar;
    }
    arena_alloc(0, string_sz + 1);
    *(alloced_dst + string_sz) = '\0';
    if (f->kind == K_ENUM) {
        if (!refl_enum_has_label(f->subtype.e, alloced_dst))
            return RET_ERR(DE_LABEL_NOT_FOUND);
    }
    refl_migrate_field(&new_field, f, (u8*)&alloced_dst, dst, 0, f->kind == K_OPTION);
reading_num:
    new_field.kind = K_PRIMITIVE;
    u64 out_bytes;
    if (generic_str_to_numeric(
        sw,
        &out_bytes,
        &f->subtype.prim
    )) {
        return RET_ERR(DE_NUMBER_CONVERSION_ERROR);
    }
    if (!refl_comp_field_schema(&new_field, f)) {
        return RET_ERR(DE_STREAM_INCOMPATIBLE);
    }
    
reading_bool:

}

static DeserializeResult qp_read_pair(
    RStruct* restrict model, 
    stream_walker* restrict sw,
    size_t* restrict key_freqs, 
    u8* restrict instance,
    size_t non_optional_counter
) {
    DeserializeError de;
    RField* field;
    size_t f_idx = (size_t)-1;
    size_t key_start_idx = sw->cur;
    char* key_start = (char*)currchar;
read_key_name:
    if (sw->cur >= sw->eof) return RET_OK(NULL);
    while(currchar != '=') {
        if (sw->cur >= sw->eof)
            return RET_ERR(DE_UNEXPECTED_EOF);
        if (!key_valid(currchar))
            return RET_ERR(DE_UNEXPECTED_CHARACTER);
        sw->cur++;
    }
    field = qp_search_field(model, ({ size_t __Sz = (sw->cur - key_start_idx); memcpy(s_cstr_buff, (key_start), __Sz); s_cstr_buff[__Sz] = '\0'; s_cstr_buff; }), &f_idx);
    if (!field)
        return RET_ERR(DE_LABEL_NOT_FOUND);

    u8* write_to = instance + field->offset;
    RField* dst_field = field; 
    int unwrapped = 0;
    key_freqs[f_idx]++;
unwrap:
    if (unwrapped >= 2) {
        return RET_ERR(DE_TARGET_TYPE_TOO_COMPLEX);
    }
    if (field->kind == K_DYN_ARRAY) {
        r_array_t* arr = (r_array_t*)write_to;
        // Alloc or realloc array
        if (key_freqs[f_idx] == 1) {
            arr->array = calloc(DYN_ARRAY_CAPACITY, field->subtype.subfield->size);
            arr->capacity = DYN_ARRAY_CAPACITY;
            arr->size = 0;
        } else if (arr->size >= arr->capacity) {
            arr->capacity <<= 1;
            arr->array = realloc(arr->array, arr->capacity);
        }
        write_to = arr->array = (key_freqs[f_idx] - 1) * field->subtype.subfield->size;
        dst_field = field->subtype.subfield; 
        unwrapped = 2;
    } else {
        if (field->kind == K_FIXED_ARRAY && (key_freqs[f_idx] <= field->size)) {
            // Get offset inside of array
            write_to += (key_freqs[f_idx] - 1) * field->subtype.subfield->size;
            unwrapped = 2;
        } else if (key_freqs[f_idx] != 1) {
            // do not append to instance if out of bounds
            goto skip_val;
        }
    } else if (field->kind == K_OPTION) {
        non_optional_counter++;
        r_option_t* opt = (r_option_t*)write_to;
        write_to = opt->optional;
        unwrapped++;
        goto unwrap;
    }

    // Read and append value
    DeserializeResult dr = qp_read_val(model, field, sw, key_freqs, field_instance);
    if (dr.st == DR_ERR) {
        return dr;
    }
    non_optional_counter--;
skip_val:
    // Advance until next key-val pair
    while(currchar != '&' && sw->cur < sw->eof )sw->cur++;
    if (sw->cur < sw->eof) { 
        sw->cur++;
        goto reading_key_name;
    }
    if (non_optional_counter != 0) {
        return RET_ERR(DE_STREAM_INCOMPATIBLE);
    }
    return RET_OK(NULL);
}

DeserializeResult deserialize_query(RStruct* restrict model, void* restrict stream, size_t stream_len) {
    if (!model || stream_len == 0) return RET_ERR(DE_NULL_PTR);
    stream_walker sw = { .buff = stream, .cur = 0, .eof = stream_len };
    size_t brk = arena_get_brk();
    u8* instance = arena_alloc(0, model->total_size);
    size_t* key_freqs = arena_alloc(0, sizeof(size_t) * model->n_elements);
    memset(key_freqs, 0, sizeof(size_t) * model->total_size);
    
    (void)sw;
    (void)brk;
    (void)uob;

    size_t non_optional_counter = 0;
    for (size_t i = 0; i < model->n_elements; i++) {
        if (model->fields[i]->kind != K_OPTION) non_optional_counter++;
    }
    

    DeserializeResult dr = qp_read_pair(model, &sw, key_freqs, instance, non_optional_counter);
    if (dr == DR_OK) {
        arena_set_brk(brk);
        if (jw.cur < jw.eof) {
            return RET_ERR(DE_EXPECTED_EOF);
        }
        void* ret = malloc(model->total_size);
        memcpy(ret, instance, model->total_size);
        return RET_OK(ret);
    }

    arena_set_brk(brk);
    return dr;
}*/

DeserializeResult deserialize_query(RStruct* restrict model, void* restrict stream, size_t stream_len) {
    (void)model;
    (void)stream_len;
    (void)stream;
    return RET_ERR(DE_NULL_PTR);   
}
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#include <public/types.h>
#include <public/cmp.h>
#include <public/core.h>
#include <public/serial/err.h>

#include <private/serial/common.h>
#include <private/arena.h>

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


#define DYN_ARRAY_CAPACITY 16

#define currchar sw->buff[sw->cur]

static inline RField* qp_search_field(RStruct* restrict m, const char* restrict name_str, size_t name_str_len, size_t* f_idx) {
    for (size_t i = 0; i < m->n_elements; i++)
    {
        RField* f = m->fields[i];
        size_t len = strlen(f->name);
        if (len == name_str_len && !memcmp(name_str, f->name, len)) { *f_idx = i; return f; }
    }
    return NULL;
}

static inline char* qp_alloc_and_url_decode_string(const char* str, size_t len) {
    char* ret = malloc(len + 1);
    char digits_buff[3] = {0};
    size_t dst_curr_off = 0;
    for (size_t i = 0; i < len; i++)
    {
        if (str[i] == '%' && (len - i) > 2) {
            digits_buff[0] = str[i + 1];
            digits_buff[1] = str[i + 2];
            i += 2;
            char* p;
            u32 byte = strtoul(digits_buff, &p, 16);
            ret[dst_curr_off++] = (byte & 0xFF);
            continue;
        }
        ret[dst_curr_off++] = str[i];
    }
    ret[dst_curr_off] = '\0';
    return ret;
}

struct mock_optional {
    bool is_some;
    void* val;
};

static inline void qp_insert_into_instance(
    RField* restrict f_schema, 
    u8* restrict instance, 
    u8* restrict val, 
    size_t val_sz,
    size_t field_freq
) {
    size_t off = f_schema->offset;
    RField* f = f_schema;
    if (f->kind == K_OPTION) {
        *(bool*)(instance + off) = true;
        f = f->subtype.subfield;
        off += offsetof(struct mock_optional, val);
    }
    if (f->kind == K_FIXED_ARRAY) {
        off += (field_freq - 1) * val_sz;
    }
    // Heap dynamic array 
    else if (f->kind == K_DYN_ARRAY) {
        r_array_t* da = (r_array_t*)(instance + off);
        if (field_freq == 1) {
            da->array = calloc(DYN_ARRAY_CAPACITY, val_sz);
            da->size = 0;
            da->capacity = DYN_ARRAY_CAPACITY;
            da->chunk_size = val_sz;
        } else if (da->size >= da->capacity) {
            da->capacity <<= 1;
            da->array = realloc(da->array, da->capacity * da->chunk_size);
            memset(da->array + (da->size * da->chunk_size), 0, (da->size * da->chunk_size));
        }
        da->size++;
        memcpy(da->array + ((field_freq - 1) * val_sz), val, val_sz);
        return;
    }
    // Regular copy
    memcpy(instance + off, val, val_sz);
}



static DeserializeResult qp_parse(
    RStruct* restrict schema, 
    stream_walker* restrict sw,
    u8* instance, 
    size_t* restrict key_freqs, 
    size_t fields_count
) {
    
    const char* str_start;
    size_t str_start_off;
    RField* field;
    size_t field_idx = (size_t)-1;
    RKind unwrapped_kind;
reading_key:
    str_start = (const char*)sw->buff + sw->cur;
    str_start_off = sw->cur;
    if (sw->cur >= sw->eof) {
        for (size_t i = 0; i < fields_count; i++) {
            RField* required = schema->fields[i];
            if (required->kind == K_OPTION) continue;
            if (key_freqs[i] == 0) return RET_ERR(DE_UNDEFINED_FIELDS);
            if (required->kind == K_FIXED_ARRAY &&
                key_freqs[i] < required->size) {
                return RET_ERR(DE_UNDEFINED_FIELDS);
            }
        }
        return RET_OK(NULL);
    }
    while(currchar != '=') {
        if (sw->cur >= sw->eof)
            return RET_ERR(DE_UNEXPECTED_EOF);
        if (!key_valid(currchar))
            return RET_ERR(DE_UNEXPECTED_CHARACTER);
        sw->cur++;
    }
    if (sw->cur == str_start_off) goto skip_val;
    field = qp_search_field(schema, str_start, (sw->cur - str_start_off), &field_idx);
    if (!field) goto skip_val;

    sw->cur++;

    key_freqs[field_idx]++;
    size_t field_holds = 1;

    // Query parameters is a simple format, it does not include nested structures.
    //  - Find target type (unwrapper_kind)
    //  - Figure out how many values that fields holds
    unwrapped_kind = field->kind;
    switch (field->kind) {
        case K_STRUCT:
        case K_UNION:
        case K_POINTER:
            return RET_ERR(DE_TARGET_TYPE_TOO_COMPLEX);
        case K_OPTION: 
            unwrapped_kind = field->subtype.subfield->kind;
            switch (unwrapped_kind) {
                case K_DYN_ARRAY: 
                    field_holds = (size_t)-1;
                    unwrapped_kind = field->subtype.subfield->subtype.subfield->kind;
                    break;
                case K_FIXED_ARRAY:
                    field_holds = field->subtype.subfield->size;
                    unwrapped_kind = field->subtype.subfield->subtype.subfield->kind;
                    break;
                default: break;
            }
            break;
        case K_DYN_ARRAY:
            field_holds = (size_t)-1;
            unwrapped_kind = field->subtype.subfield->kind;
            break;
        case K_FIXED_ARRAY:
            field_holds = field->size;
            unwrapped_kind = field->subtype.subfield->kind;
            break;
        default:
            break;
    }

    if (key_freqs[field_idx] > field_holds) goto skip_val;

    switch (unwrapped_kind) {
        case K_STRING:
        // TODO: Enum label and numeric conversion
        // case K_PRIMITIVE:
        // case K_ENUM: 
            break;
        default: return RET_ERR(DE_TARGET_TYPE_TOO_COMPLEX);
    }

    // Only string for now
    str_start = (const char*)sw->buff + sw->cur;
    str_start_off = sw->cur;
    while(currchar != '&' && sw->cur < sw->eof) sw->cur++;
    char* field_val = qp_alloc_and_url_decode_string(str_start, (sw->cur - str_start_off));
    // Insert pointer into instance
    qp_insert_into_instance(field, instance, (u8*)&field_val, sizeof(void*), key_freqs[field_idx]);
    if (sw->cur < sw->eof) sw->cur++;
    goto reading_key;

skip_val:
    while(currchar != '&' && sw->cur < sw->eof) sw->cur++;
    sw->cur++;
    goto reading_key;
}

DeserializeResult query_deserialize(RStruct* restrict schema, void* restrict stream, size_t stream_len) {
    if (!schema || stream_len == 0) return RET_ERR(DE_NULL_PTR);
    stream_walker sw = { .buff = stream, .cur = 0, .eof = stream_len };
    size_t brk = arena_get_brk();
    u8* instance = arena_alloc(0, schema->total_size);
    memset(instance, 0, schema->total_size);
    size_t* key_freqs = arena_alloc(0, sizeof(size_t) * schema->n_elements);
    memset(key_freqs, 0, sizeof(size_t) * schema->n_elements);
    
    (void)sw;
    (void)brk;

    DeserializeResult dr = qp_parse(schema, &sw, instance, key_freqs, schema->n_elements);
    if (dr.st == DR_OK) {
        if (sw.cur < sw.eof) {
            arena_set_brk(brk);
            return RET_ERR(DE_EXPECTED_EOF);
        }
        void* ret = malloc(schema->total_size);
        memcpy(ret, instance, schema->total_size);
        arena_set_brk(brk);
        return RET_OK(ret);
    }

    arena_set_brk(brk);
    return dr;
}

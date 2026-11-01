#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#include <public/types.h>
#include <public/cmp.h>
#include <public/core.h>
#include <public/serial/err.h>

#include <private/serial/common.h>
#include <private/arena.h>

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

static void query_url_encode(
    serializer_string_buffer* buffer,
    const char* string
) {
    static const char hex[] = "0123456789ABCDEF";
    for (const unsigned char* current = (const unsigned char*)string;
         *current;
         current++) {
        unsigned char c = *current;
        int ascii_alphanumeric =
            (c >= 'A' && c <= 'Z') ||
            (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9');
        if (ascii_alphanumeric || c == '+' || c == '-' || c == '_' || c == '.') {
            serializer_append(buffer, (const char*)current, 1);
        } else {
            char encoded[] = { '%', hex[c >> 4], hex[c & 0x0F] };
            serializer_append(buffer, encoded, sizeof(encoded));
        }
    }
}


size_t query_serialize(RStruct* s, RObject instance, void** out) {
    SerializePolicy sp = {
        .arr_end = "",
        .arr_start = "",
        .arr_sep = "",
        .enum_as_string = 1,
        .field_sep = "&",
        .format_primitive = generic_numeric_to_str,
        .indent_str = "",
        .key_end = "",
        .key_start = "",
        .scape_keys = 0,
        .key_val_sep = "=",
        .newline = "",
        .obj_end = "",
        .obj_start = "",
        .opt_none = NULL,
        .opt_some_start = NULL,
        .opt_some_end = NULL,
        .unwrap_optionals = 1,
        .repeat_array_fields = 1,
        .pretty = 0,
        .str_end = "",
        .str_start = "",
        .null_ptr = "0",
        .null_str = "null",
        .string_encode = query_url_encode
    };
    if (out) {
        return walk_type(&sp, s, instance, out);
    }
    return 0;
}



static inline RField* qp_search_field(RStruct* restrict m, const char* restrict name_str, size_t name_str_len, size_t* f_idx) {
    for (size_t i = 0; i < m->n_elements; i++)
    {
        RField* f = m->fields[i];
        size_t len = strlen(f->name);
        if (len == name_str_len && !memcmp(name_str, f->name, len)) { *f_idx = i; return f; }
    }
    return NULL;
}

static inline char* qp_url_decode_string(
    const char* restrict str,
    size_t len,
    size_t* restrict out_len,
    bool malloc_it
) {
    char* ret = (malloc_it)? malloc(len + 1) : arena_alloc(0, len + 1);
    if (!ret) return NULL;
    if (!malloc_it && arena_memory_failed()) return NULL;
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
    *out_len = dst_curr_off;
    return ret;
}

static inline int qp_insert_into_instance(
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
        off += f->offset;
    }
    if (f->kind == K_FIXED_ARRAY) {
        off += (field_freq - 1) * val_sz;
    }
    // Heap dynamic array 
    else if (f->kind == K_DYN_ARRAY) {
        r_array_t* da = (r_array_t*)(instance + off);
        if (field_freq == 1) {
            da->array = calloc(DYN_ARRAY_CAPACITY, val_sz);
            if (!da->array) return 0;
            da->size = 0;
            da->capacity = DYN_ARRAY_CAPACITY;
            da->chunk_size = val_sz;
        } else if (da->size >= da->capacity) {
            size_t old_capacity = da->capacity;
            size_t new_capacity = old_capacity * 2;
            void* new_array = realloc(da->array, new_capacity * da->chunk_size);
            if (!new_array) return 0;
            da->array = new_array;
            da->capacity = new_capacity;
            memset(
                (u8*)da->array + old_capacity * da->chunk_size,
                0,
                (new_capacity - old_capacity) * da->chunk_size
            );
        }
        da->size++;
        memcpy((u8*)da->array + ((field_freq - 1) * val_sz), val, val_sz);
        return 1;
    }
    // Regular copy
    memcpy(instance + off, val, val_sz);
    return 1;
}

static inline int qp_num_convert(
    RPrimitive prim_expected,
    const char* restrict str,
    size_t str_len,
    u64* converted
) {
    u64 observed_number;
    RPrimitive observed_prim;
    stream_walker s = (stream_walker){ .buff = (u8*)str, .cur = 0, .eof = str_len };
    if (!generic_str_to_numeric(
        &s,
        &observed_number,
        &observed_prim
    ) || s.cur != s.eof) return 0;
    if (!refl_primitive_can_promote(observed_prim, prim_expected))
        return 0;

    *converted = 0;
    refl_primitive_convert(
        observed_prim,
        prim_expected,
        &observed_number,
        converted
    );
    return 1;
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
    RField* value_field;
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
    value_field = field;
    switch (field->kind) {
        case K_STRUCT:
        case K_UNION:
        case K_POINTER:
            return RET_ERR(DE_TARGET_TYPE_TOO_COMPLEX);
        case K_OPTION: 
            value_field = field->subtype.subfield;
            unwrapped_kind = value_field->kind;
            switch (unwrapped_kind) {
                case K_DYN_ARRAY: 
                    field_holds = (size_t)-1;
                    value_field = value_field->subtype.subfield;
                    unwrapped_kind = value_field->kind;
                    break;
                case K_FIXED_ARRAY:
                    field_holds = value_field->size;
                    value_field = value_field->subtype.subfield;
                    unwrapped_kind = value_field->kind;
                    break;
                default: break;
            }
            break;
        case K_DYN_ARRAY:
            field_holds = (size_t)-1;
            value_field = field->subtype.subfield;
            unwrapped_kind = value_field->kind;
            break;
        case K_FIXED_ARRAY:
            field_holds = field->size;
            value_field = field->subtype.subfield;
            unwrapped_kind = value_field->kind;
            break;
        default:
            break;
    }

    if (key_freqs[field_idx] > field_holds) goto skip_val;

    switch (unwrapped_kind) {
        case K_STRING:
        case K_PRIMITIVE:
        case K_ENUM:
            break;
        default: return RET_ERR(DE_TARGET_TYPE_TOO_COMPLEX);
    }

    str_start = (const char*)sw->buff + sw->cur;
    str_start_off = sw->cur;
    while(currchar != '&' && sw->cur < sw->eof) sw->cur++;
    size_t len;
    void* field_val = qp_url_decode_string(
        str_start,
        (sw->cur - str_start_off),
        &len,
        unwrapped_kind == K_STRING
    );
    if (!field_val) return RET_ERR(DE_MEMORY_ERROR);

    size_t field_sz;
    void* value_to_insert;
    u64 converted;
    int enum_value;
    if (unwrapped_kind == K_PRIMITIVE) {
        // Convert to number
        if (!qp_num_convert(value_field->subtype.prim, field_val, len, &converted))
            return RET_ERR(DE_NUMBER_CONVERSION_ERROR);
        value_to_insert = &converted;
        field_sz = refl_prim_to_size(value_field->subtype.prim);

    } else if (unwrapped_kind == K_ENUM) {
        // Convert to enum value if label is present
        REnum* enum_type = value_field->subtype.e;
        size_t enum_index;
        for (enum_index = 0; enum_index < enum_type->n_elements; enum_index++) {
            if (strcmp(field_val, enum_type->names[enum_index]) == 0) break;
        }
        if (enum_index == enum_type->n_elements)
            return RET_ERR(DE_LABEL_NOT_FOUND);
        enum_value = (int)enum_type->indexes[enum_index];
        value_to_insert = &enum_value;
        field_sz = refl_get_field_size(value_field);
    } else {
        value_to_insert = &field_val;
        field_sz = sizeof(field_val);
    }

    if (!qp_insert_into_instance(
        field,
        instance,
        value_to_insert,
        field_sz,
        key_freqs[field_idx]
    )) {
        if (unwrapped_kind == K_STRING) free(field_val);
        return RET_ERR(DE_MEMORY_ERROR);
    }
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
    size_t arena_brk = arena_get_brk();
    u8* instance = calloc(1, schema->total_size);
    if (!instance) return RET_ERR(DE_MEMORY_ERROR);
    size_t* key_freqs = arena_alloc(0, sizeof(size_t) * schema->n_elements);
    if (arena_memory_failed()) {
        arena_set_brk(arena_brk);
        free(instance);
        return RET_ERR(DE_MEMORY_ERROR);
    }

    DeserializeResult dr = qp_parse(schema, &sw, instance, key_freqs, schema->n_elements);
    if (dr.st == DR_OK) {
        if (sw.cur < sw.eof) {
            refl_generic_destructor(schema, instance);
            arena_set_brk(arena_brk);
            return RET_ERR(DE_EXPECTED_EOF);
        }
        arena_set_brk(arena_brk);
        return RET_OK(instance);
    }

    refl_generic_destructor(schema, instance);
    arena_set_brk(arena_brk);
    return dr;
}

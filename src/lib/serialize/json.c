#define _GNU_SOURCE

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <stdbool.h>

#include <crefl/types.h>
#include <crefl/cmp.h>
#include <crefl/core.h>

#include <serial/_common.h>
#include <serial/err.h>
#include <_arena.h>



char* deserialize_strerror(DeserializeError e);

#define MAX(a, b) ((a) > (b))? (a) : (b)
#define MIN(a, b) ((a) < (b))? (a) : (b)
#define MIN3(a, b, c) MIN(MIN(a, b), c)

#define skip_chars(_char) ({\
    int __ret = 2;\
    switch (_char)\
    {\
    case '\n':__ret=1;break;\
    case ' ' :__ret=1;break;\
    case '\t':__ret=1;break;\
    default:__ret=0;break;\
    }\
    __ret;\
})

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



#define RET_ERR(_err) (DeserializeResult){.st = DR_ERR, .err = _err, .data.err_str=deserialize_strerror(_err)}
#define RET_OK(_ptr) (DeserializeResult){.st = DR_OK, .data.buff = _ptr}

#define FIELD_POOL_INITIAL 4
#define ARRAY_SIZE_INITIAL 8
#define INSTANCE_INITIAL 8



#define currchar jw->buff[jw->cur]


void* serialize_json(RStruct* s, RObject instance) {
    SerializePolicy sp = {
        .arr_end = " ]",
        .arr_start = "[ ",
        .arr_sep = ", ",
        .enum_as_string = 0,
        .field_sep = ", ",
        .format_primitive = generic_numerical_converter,
        .indent_str = "    ",
        .key_end = "\"",
        .key_start = "\"",
        .scape_keys = 1,
        .key_val_sep = " : ",
        .newline = "\n",
        .obj_end = "}",
        .obj_start = "{",
        .opt_none = NULL,
        .opt_some_start = NULL,
        .opt_some_end = NULL,
        .unwrap_optionals = 1,
        .pretty = 1,
        .str_end = "\"",
        .str_start = "\""
    };
    return walk_type(&sp, s, instance);
}


static inline RPrimitive _smallest_primitive_type_unsigned(uint64_t v, uint8_t* s) {
    *s = 0;
    if (v <= UINT8_MAX)  {*s = 1;return P_U8;}
    if (v <= UINT16_MAX) {*s = 2;return P_U16;}
    if (v <= UINT32_MAX) {*s = 4;return P_U32;}
    if (v <= UINT64_MAX) {*s = 8;return P_U64;}
    return P_U0;
}

static inline RPrimitive _smallest_primitive_type_signed(int64_t v, uint8_t* s) {
    *s = 0;
    if (v <= INT8_MAX)  {*s = 1;return P_I8;}
    if (v <= INT16_MAX) {*s = 2;return P_I16;}
    if (v <= INT32_MAX) {*s = 4;return P_I32;}
    if (v <= INT64_MAX) {*s = 8;return P_I64;}
    return P_U0;
}

static int _json_str_to_numeric(
    uint8_t* buff, 
    size_t* _cur, 
    size_t eof,
    uint64_t* out_bytes,
    RPrimitive* out_prim_kind  
) {
    size_t start=*_cur;
    size_t cur = *_cur;
    uint8_t is_float=0;
    uint8_t sign_bit=0;

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
        cur++;
        while(cur < eof && isdigit(buff[cur])) cur++;
    }

    // cur should point at the end of the ASCII representation by now.
    *_cur = cur;
    char replacing = buff[cur];
    buff[cur] = '\0';
    uint8_t s;
    errno = 0;

    if (is_float) {
        double d = strtod((const char*)buff + start, NULL);
        if (errno) return 0;
        float f = (float)d;
        // Return P_F32 if lossless
        if (f == d) {
            *out_prim_kind = P_F32;
            memcpy(out_bytes, &f, sizeof(float));
        } else {
            *out_prim_kind = P_F64;
            memcpy(out_bytes, &d, sizeof(double));
        }
    } else if (sign_bit) {
        int64_t v = strtol((char*)buff + start, NULL, 10);
        if (errno) return 0;
        *out_prim_kind = _smallest_primitive_type_signed(v, &s);
        memcpy(&out_bytes, &v, s);
    } else {
        uint64_t v = strtoul((char*)buff + start, NULL, 10);
        if (errno) return 0;
        *out_prim_kind = _smallest_primitive_type_unsigned(v, &s);
        memcpy(out_bytes, &v, s);
    }
    buff[cur] = replacing;
    return 1;
}

typedef struct {
    uint8_t* buff;
    size_t cur;
    size_t eof;
} json_walker;

struct _json_nesting_stack {
    uint8_t stack[512];
    uint32_t sp;
    int overflown;
};

static inline void _jss_push(struct _json_nesting_stack* jss, uint8_t c) {
    jss->stack[jss->sp++ & 0x1FF] = c;
    if (jss->sp & ~0x1FF) jss->overflown = 1;
}

/**
 * You can only pop if c is the same char at stack top.
 */
static inline void _jss_pop(struct _json_nesting_stack* jss, uint8_t c) {
    if (c == jss->stack[jss->sp - 1]) jss->sp--;
}

static DeserializeResult __ds_obj(
    json_walker* jw, 
    struct _json_nesting_stack* jss, 
    unordered_buffer* instance,
    temp_field_pool* fp,
    int type_data_partition
);

static DeserializeResult __ds_val(
    json_walker* jw,
    struct _json_nesting_stack* jss,
    temp_field_pool* fp,
    size_t* curr_offset,
    unordered_buffer* instance,
    size_t* write_at,
    int type_data_partition
) {
    DeserializeError de;
    while (skip_chars(currchar)) jw->cur++;
    RField* curr = NULL;
    size_t size;
    size_t substr_start;
    uint64_t raw;           // Raw bytes output from numerical types
    bool bool_val = 0;
    bool scaped;            // scaping chars like \" \n \t
    switch (currchar)
    {
    case '\"':
        substr_start = ++jw->cur;
        goto reading_string;
    
    case 't':
        if (memcmp(jw->buff + jw->cur, "true", 4) == 0) {
            bool_val = 1;
            jw->cur += 4;
            goto reading_bool;
        }
        de = DE_UNKNOWN_KEYWORD; goto return_err;
    case 'f':
        if (memcmp(jw->buff + jw->cur, "false", 5) == 0) {
            bool_val = 0;
            jw->cur += 5;
            goto reading_bool;
        } 
        de = DE_UNKNOWN_KEYWORD; goto return_err;

    case '{':
        goto reading_struct;

    default:
        if (num_detect(currchar)) {
            goto reading_numeric;
        } else {
            de = DE_UNEXPECTED_CHARACTER; goto return_err;
        }
        break;
    }

reading_struct:
    _jss_push(jss, 0);
    jw->cur++;
    {
        RField e = {0};
        _fp_append(fp, &e);
        curr = &fp->f[fp->sz - 1];
        *write_at = fp->sz - 1;
        DeserializeResult dr = __ds_obj(jw, jss, instance, fp, type_data_partition);
        if (dr.st == DR_OK) {
            RStruct* s = ((RStruct*)dr.data.buff);
            curr->kind = K_STRUCT;
            curr->offset = *curr_offset;
            *curr_offset += s->total_size;
            curr->next_offset = *curr_offset;
            curr->subtype.s = s;
            jw->cur++;
            goto finish_field;
        } else {
            return dr;
        }
    }

reading_bool:
    curr = arena_alloc(0, sizeof(RField));
    curr->kind = K_PRIMITIVE;
    curr->subtype.prim = P_BOOL;
    size = refl_prim_to_size(P_BOOL);
    curr->offset = *curr_offset;
    *curr_offset += size;
    curr->next_offset = *curr_offset;
    _uob_append(instance, &bool_val, size);
    goto finish_field;

reading_numeric:
    curr = arena_alloc(0, sizeof(RField));
    curr->kind = K_PRIMITIVE;
    if (_json_str_to_numeric(
        jw->buff,
        &jw->cur,
        jw->eof,
        &raw,
        &curr->subtype.prim
    ) == 0) { 
        de = DE_NUMBER_CONVERSION_ERROR; 
        goto return_err;
    }
    size = refl_prim_to_size(curr->subtype.prim);
    curr->offset = *curr_offset;
    *curr_offset += size;
    curr->next_offset = *curr_offset;
    _uob_append(instance, &raw, size);
    goto finish_field;
    


reading_string:
    curr = arena_alloc(0, sizeof(RField));
    curr->kind = K_STRING;
    curr->size = 1;
    curr->offset = *curr_offset;
    *curr_offset += sizeof(char*);
    curr->next_offset = *curr_offset;
    // Unused subtype
    while (jw->cur < jw->eof) {
        if (currchar == '\"' && scaped == 0) {
            currchar = '\0';
            // Write pointer
            uint8_t* str = jw->buff + substr_start;
            jw->cur++;
            _uob_append(instance, &str, sizeof(char*));
            goto finish_field;
        }
        // Actual scaping of characters will be handled later down the line.
        if (scaped == 1) scaped = 0;       // Only lasts one character   
        if (currchar == '\\') scaped ^= 1; // flip flag
        
        jw->cur++;
    }
    de = DE_UNCLOSED_DQUOTE; goto return_err;

finish_field:
    DeserializeResult dr = {.data.buff = curr, .st = DR_OK};
    return dr;

return_err:
    arena_dealloc(0, sizeof(RField));
    return RET_ERR(de);

}


/**
 * @brief State machine deserialization of a JSON object.
 */
static DeserializeResult __ds_obj(
    json_walker* jw, 
    struct _json_nesting_stack* jss, 
    unordered_buffer* instance,
    temp_field_pool* fp,
    int type_data_partition
) {
    size_t substr_start=(size_t)-1;
    int expecting_more=0;   // last field ended with ',' -> we expect more field.
    size_t curr_offset = 0; // the instance and type we are building are going to be packed.
    size_t n_elements=0;
    size_t field_start = fp->sz;
    size_t starting_sp = jss->sp;
    size_t write_at = (size_t)-1;
    RField* f = NULL;
    
    DeserializeError de;
    while (jw->cur < jw->eof)
    {
        char* field_name;
        if (skip_chars(currchar)) { jw->cur++; continue; }
        if (currchar == '}') {
            if (expecting_more) {
                de = DE_UNEXPECTED_CHARACTER;
                goto return_err;
            } else {
                _jss_pop(jss, 0);
                break;
            }
        }
        expecting_more = 0;
reading_key:
        switch (currchar)
        {
        case '\"':
            if (substr_start == (size_t)-1) {
                substr_start = ++jw->cur;
                goto reading_key;
            }
            goto carve_key;
        
        default:
            if (!key_valid(currchar)) { 
                de = DE_UNEXPECTED_CHARACTER;
                goto return_err;
            }
            jw->cur++; goto reading_key;
        }

reading_value:
        while (skip_chars(currchar)) jw->cur++;
        if (currchar != ':') { 
            de = DE_UNEXPECTED_CHARACTER; goto return_err;}
        jw->cur++;
        while (skip_chars(currchar)) jw->cur++;
        DeserializeResult dr = __ds_val(jw, jss, fp, &curr_offset, instance, &write_at, type_data_partition);
        if (dr.st == DR_ERR) {
            return dr;
        }

        f = dr.data.buff;
        if (write_at != (size_t)-1) {
            (&fp->f[write_at])->name = (const char*)field_name; 
        } else {
            f->name = (const char*)field_name;
        }
        goto finish_field;

carve_key:
        currchar = '\0';
        field_name = (char*)jw->buff + substr_start;
        jw->cur++;
        substr_start = (size_t)-1;
        goto reading_value;

finish_field:
        // Skip until ending ','
        while (skip_chars(currchar)) jw->cur++;
        n_elements++;

        if (write_at == (size_t)-1) {
            _fp_append(fp, f); 
            arena_dealloc(0, sizeof(RField));
        }
        else write_at = (size_t)-1;
        if (arena_memory_failed()) { 
            de = DE_MEMORY_ERROR; 
            visualize_arena();
            goto return_err;}
        if (currchar == '}') {
            continue;
        }
        else if (currchar != ',') { 
            de = DE_UNEXPECTED_CHARACTER;
            goto return_err;
        }
        expecting_more = 1;
        jw->cur++;
        continue;

return_err:
        return RET_ERR(de);
    }

    // Final syntax checks
    if (jss->overflown != 0) {
        return RET_ERR(DE_TOO_MUCH_NESTING);
    }
    if (jss->sp != starting_sp - 1) { 
        return RET_ERR(DE_UNBALANCED_STREAM);
    }

    // Make the struct
    RStruct* generated = arena_alloc(type_data_partition, sizeof(RStruct));
    *generated = (RStruct){
        .n_elements = n_elements,
        .name_type = "placeholder",
        .total_size = curr_offset,
        .fields = arena_alloc(type_data_partition, sizeof(RField*) * n_elements)
    };
    if (arena_memory_failed()) {
        return RET_ERR(DE_MEMORY_ERROR);
    }
    for (size_t i = 0; i < n_elements; i++)
    {   
        generated->fields[i] = fp->f + i + field_start;
        // Skip other struct's fields
        if (fp->f->kind == K_STRUCT) {
            field_start += fp->f->subtype.s->n_elements;
        }
    }
    return RET_OK(generated);
}

// UL -> UPPERLIMIT
#define UL_FIELD_POOL(len)  MAX(256, 12 * len)       //12n
#define UL_INSTANCE(len)    MAX(32, len)             //n
#define UL_TYPE_DATA(len)   MAX(sizeof(RStruct) + sizeof(void*), len / 6 * 10) //0.6n

/**
 * @brief Attempt to deserialize a json stream into given type.
 * @returns Result, either the data or an error.
 * @attention non-homogeneus arrays are not allowed for now.
 */
DeserializeResult deserialize_json(RStruct* model, void* stream) {
    if (!model) return RET_ERR(DE_NULL_PTR);
    json_walker jw = { .cur = 0, .eof = strlen((char*)stream) };
    size_t brk = arena_get_brk();
    jw.buff = arena_alloc(0, jw.eof + 1);
    memcpy(jw.buff, stream, jw.eof + 1);
    struct _json_nesting_stack jss = {.stack = {0}, .overflown = 0, .sp = 0};

    DeserializeResult ret = { 0 };
    unordered_buffer instance = _uob_init(arena_create_partition(UL_INSTANCE(jw.eof)), INSTANCE_INITIAL);
    temp_field_pool fp = _fp_init(arena_create_partition(UL_FIELD_POOL(jw.eof)), FIELD_POOL_INITIAL);
    int type_data = arena_create_partition(UL_TYPE_DATA(jw.eof));
    
    while(jw.cur < jw.eof)
    {
        char c = jw.buff[jw.cur];
        if (skip_chars(c)) { jw.cur++; continue; }
        if (c == '{') {
            _jss_push(&jss, 0);
            jw.cur++;
            ret = __ds_obj(&jw, &jss, &instance, &fp, type_data);
            break;
        } else {
            _uob_free(&instance);
            arena_delete_partitions(3);
            arena_dealloc(0, jw.eof + 1);
            return RET_ERR(DE_STREAM_INCOMPATIBLE);
        }
        jw.cur++;
    }

    if (ret.st == DR_OK) {
        // Check compatibilty and migrate
        RStruct* generated = ret.data.buff;

        visualize_arena();

        if (refl_comp_struct_schema(generated, model) == 0) {
            _uob_free(&instance);
            arena_delete_partitions(3);
            arena_set_brk(brk);
            return RET_ERR(DE_STREAM_INCOMPATIBLE);
        }
        uint8_t* result = refl_struct_migrate(generated, model, instance.data);
        _uob_free(&instance);
        arena_delete_partitions(3);
        arena_set_brk(brk);
        return RET_OK(result);
    }


    _uob_free(&instance);
    arena_set_brk(brk);
    arena_delete_partitions(3);
    return ret;
}
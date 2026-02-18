#define _GNU_SOURCE

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <assert.h>

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
        is_float = 1;
        cur++;
        while(cur < eof && isdigit(buff[cur])) cur++;
    }

    // cur should point at the end of the ASCII representation by now.
    *_cur = cur;
    char* extracted = arena_alloc(0, cur - start + 1);
    memcpy(extracted, buff + start, cur - start);
    uint8_t s;
    errno = 0;

    if (is_float) {
        double d = strtod((const char*)extracted, NULL);
        if (errno) return 0;
        float f = (float)d;
        // Return P_F32 if lossless
        if (f == d) {
            *out_prim_kind = P_F32;
            memset(out_bytes, 0, sizeof(uint64_t));
            memcpy(out_bytes, &f, sizeof(float));
        } else {
            *out_prim_kind = P_F64;
            memset(out_bytes, 0, sizeof(uint64_t));
            memcpy(out_bytes, &d, sizeof(double));
        }
    } else if (sign_bit) {
        int64_t v = strtol((char*)extracted, NULL, 10);
        if (errno) return 0;
        *out_prim_kind = _smallest_primitive_type_signed(v, &s);
        memset(out_bytes, 0, sizeof(uint64_t));
        memcpy(out_bytes, &v, s);
    } else {
        uint64_t v = strtoul((char*)extracted, NULL, 10);
        if (errno) return 0;
        *out_prim_kind = _smallest_primitive_type_unsigned(v, &s);
        memset(out_bytes, 0, sizeof(uint64_t));
        memcpy(out_bytes, &v, s);
    }
    arena_dealloc(0, cur - start + 1);
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
    bool overflown;
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


static RField* _json_is_name_present(
    RStruct* m, 
    const char* name, 
    size_t str_len
) {
    for (size_t i = 0; i < m->n_elements; i++)
    {
        if (!memcmp(name, m->fields[i]->name, str_len)) {
            if (m->fields[i]->name[str_len] == 0)return m->fields[i];
        }
    }
    return NULL;
}

static DeserializeResult _json_dsobj(
    RStruct* model,
    json_walker* jw, 
    struct _json_nesting_stack* jss,
    size_t offset,
    unordered_buffer* instance
);

static DeserializeResult _json_dsarr(
    RField* array,
    json_walker* jw, 
    struct _json_nesting_stack* jss,
    size_t offset,
    unordered_buffer* instance
);

static DeserializeResult _json_dsval(
    RField* model,
    json_walker* jw,
    struct _json_nesting_stack* jss,
    size_t offset,
    unordered_buffer* instance,
    size_t* non_optional_counter
) {
    (void)jss;
    while (skip_chars(currchar)) jw->cur++;
    RField curr = {0};
    size_t str_len = 0;
    size_t substr_start;
    uint64_t raw;           // Raw bytes output from numerical types
    uint8_t bool_val = 0;
    bool scaped=0;          // scaping chars like \" \n \t
    bool wrapped_dst = model->kind == K_OPTION;
    if (!wrapped_dst && non_optional_counter) (*non_optional_counter)++;
    uint8_t* dst = instance->data + offset + model->offset;
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
        return RET_ERR(DE_UNKNOWN_KEYWORD);
    case 'f':
        if (memcmp(jw->buff + jw->cur, "false", 5) == 0) {
            bool_val = 0;
            jw->cur += 5;
            goto reading_bool;
        } 
        return RET_ERR(DE_UNKNOWN_KEYWORD);

    case '{':
        goto reading_struct;

    case '[':
        goto reading_array;

    default:
        if (num_detect(currchar)) {
            goto reading_numeric;
        } else {
            return RET_ERR(DE_UNEXPECTED_CHARACTER);
        }
        break;
    }

reading_array:
    _jss_push(jss, 1);
    jw->cur++;
    {
        if (model->kind != K_DYN_ARRAY && model->kind != K_FIXED_ARRAY) {
            return RET_ERR(DE_STREAM_INCOMPATIBLE);
        }
        DeserializeResult dr = _json_dsarr(model, jw, jss, offset + model->offset, instance);
        if (dr.st == DR_OK) {
            jw->cur++;
            goto finish_field;
        }  else {
            return RET_ERR(dr.err);
        }
    }

reading_struct:
    _jss_push(jss, 0);
    jw->cur++;
    {
        if (model->kind != K_STRUCT) return RET_ERR(DE_STREAM_INCOMPATIBLE);
        DeserializeResult dr = _json_dsobj(model->subtype.s, jw, jss, offset + model->offset, instance);
        if (dr.st == DR_OK) {
            jw->cur++;
            goto finish_field;
        } else {
            return RET_ERR(dr.err);
        }
    }

reading_bool:
    curr.kind = K_PRIMITIVE;
    curr.subtype.prim = P_BOOL;
    if (!refl_comp_field_schema(&curr, model)) {
        return RET_ERR(DE_STREAM_INCOMPATIBLE);
    }
    refl_migrate_field(&curr, model, &bool_val, dst, 0, wrapped_dst);
    goto finish_field;

reading_numeric:
    curr.kind = K_PRIMITIVE;
    if (_json_str_to_numeric(
        jw->buff,
        &jw->cur,
        jw->eof,
        &raw,
        &curr.subtype.prim
    ) == 0) { 
        return RET_ERR(DE_NUMBER_CONVERSION_ERROR);
    }
    if (!refl_comp_field_schema(&curr, model)) {
        return RET_ERR(DE_STREAM_INCOMPATIBLE);
    }
    refl_migrate_field(&curr, model, (uint8_t*)&raw, dst, 0, wrapped_dst);
    goto finish_field;
    


reading_string:
    curr.kind = K_STRING;
    curr.size = 1;
    if (!refl_comp_field_schema(&curr, model)) 
        return RET_ERR(DE_STREAM_INCOMPATIBLE);
    // Unused subtype
    while (jw->cur < jw->eof) {
        if (currchar == '\"' && scaped == 0) {
            currchar = '\0';
            // Write pointer
            uint8_t* str = jw->buff + substr_start;
            jw->cur++;
            char* extracted = arena_alloc(0, str_len + 1);
            memcpy(extracted, str, str_len);
            refl_migrate_field(&curr, model, (uint8_t*)&extracted, dst, 0, wrapped_dst);
            arena_dealloc(0, str_len + 1);
            goto finish_field;
        }
        // Actual scaping of characters will be handled later down the line.
        if (scaped == 1) scaped = 0;       // Only lasts one character   
        if (currchar == '\\') scaped ^= 1; // flip flag
        str_len++;
        jw->cur++;
    }
    return RET_ERR(DE_UNCLOSED_DQUOTE);

finish_field:
    return RET_OK(NULL);


}

/** 
 * cursor expected at: [     1, 2, 3, ..., ]
 *                      ^^^^
 *                      SOMEWHERE HERE
 */
static DeserializeResult _json_dsarr(
    RField* array,
    json_walker* jw, 
    struct _json_nesting_stack* jss,
    size_t offset,
    unordered_buffer* instance
) {
    while(skip_chars(currchar))jw->cur++;

    // Handle empty arrays separately
    if (currchar == ']') {
        _jss_pop(jss, 1);
        return RET_OK(NULL);
    }

    size_t sz =  refl_get_field_size(array->subtype.subfield);
    size_t n_elements = 0;
    uint8_t* arr_start = NULL;

    

    if (array->kind == K_DYN_ARRAY) {  
        while (jw->cur < jw->eof) {
            while(skip_chars(currchar))jw->cur++;
            uint8_t* write_field_at = arena_alloc(0, sz);
            if (!n_elements) arr_start = write_field_at;
            n_elements++;
            unordered_buffer fake_instance = {.data = write_field_at}; 
            DeserializeResult dr = _json_dsval(
                array->subtype.subfield, jw, jss, 
                0, 
                &fake_instance, NULL);
            if (dr.st == DR_ERR) {
                arena_dealloc(0, sz * n_elements);
                return RET_ERR(dr.err);   
            }
            while(skip_chars(currchar))jw->cur++;
            if (currchar == ',') { jw->cur++;continue;}
            break;
        }
        if (currchar == ']') _jss_pop(jss, 1); 
        r_array_t rarr = {.capacity = n_elements, .size = n_elements, .chunk_size = sz};
        rarr.array = calloc(sz, n_elements);
        memcpy(rarr.array, arr_start, sz * n_elements);
        memcpy(instance->data + offset, &rarr, sizeof(r_array_t));
        arena_dealloc(0, sz * n_elements);
    } else {
        while (jw->cur < jw->eof) {
            while(skip_chars(currchar))jw->cur++;
            unordered_buffer fake_instance = {
                .data = instance->data + offset + (sz * n_elements)
            };
            DeserializeResult dr = _json_dsval(
                array->subtype.subfield,
                jw,
                jss,
                0,
                &fake_instance,
                NULL
            );
            n_elements++;
            if (dr.st == DR_ERR) {
                arena_dealloc(0, sz * n_elements);
                return RET_ERR(dr.err);   
            }
            while(skip_chars(currchar))jw->cur++;
            if (currchar == ',') { jw->cur++;continue;}
            break;
        }
        if (currchar == ']') _jss_pop(jss, 1);
    }

    return RET_OK(NULL);

    
}


/**
 * @brief State machine deserialization of a JSON object.
 */
static DeserializeResult _json_dsobj(
    RStruct* model,
    json_walker* jw, 
    struct _json_nesting_stack* jss,
    size_t offset,
    unordered_buffer* instance
) {
    size_t substr_start=(size_t)-1;
    size_t str_len=0;
    int expecting_more=0;   // last field ended with ',' -> we expect more field.
    size_t n_elements=0;
    size_t curr_nest_lvl = jss->sp;
    RField* model_curr = NULL;
    size_t non_optinal_counter = 0;
    
    DeserializeError de;
init:
    while (jw->cur < jw->eof)
    {
        char* field_name;
        while (skip_chars(currchar)) jw->cur++;
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
        str_len = 0;
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
            str_len++;
            jw->cur++; goto reading_key;
        }

reading_value:
        while (skip_chars(currchar)) jw->cur++;
        if (currchar != ':') { 
            de = DE_UNEXPECTED_CHARACTER; goto return_err;}
        jw->cur++;
        while (skip_chars(currchar)) jw->cur++;
        DeserializeResult dr = _json_dsval(model_curr, jw, jss, offset, instance, &non_optinal_counter);
        if (dr.st == DR_ERR) {
            de = dr.err; goto return_err;
        }
        goto finish_field;

carve_key:
        field_name = (char*)jw->buff + substr_start;
        model_curr = _json_is_name_present(model, field_name, str_len); 
        if (!model_curr) {
            substr_start = (size_t)-1;
            goto skip_field;
        }
        jw->cur++;
        substr_start = (size_t)-1;
        goto reading_value;

skip_field:
        while(jw->cur < jw->eof) {
            switch (currchar)
            {
            case '[':_jss_push(jss, 1);break;
            case '{':_jss_push(jss, 0);break;
            case ']':_jss_pop(jss, 1);break;
            case '}':
                if (jss->sp == curr_nest_lvl) {
                    expecting_more = 0;
                    goto init;
                }
                _jss_pop(jss, 0);
                break;
            case ',':
                if (jss->sp == curr_nest_lvl) {
                    expecting_more = 1;
                    jw->cur++;
                    goto init;
                } break;
            default:
                break;
            }
            jw->cur++;
        }
        de = DE_UNEXPECTED_EOF; goto return_err;

finish_field:
        // Skip until ending ','
        while (skip_chars(currchar)) jw->cur++;
        n_elements++;

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
    if (jss->sp != curr_nest_lvl - 1) { 
        return RET_ERR(DE_UNBALANCED_STREAM);
    }

    size_t c = 0;
    for (size_t i = 0; i < model->n_elements; i++)
    {
        if (model->fields[i]->kind != K_OPTION)c++;
    }
    if (non_optinal_counter < c)
        return RET_ERR(DE_STREAM_INCOMPATIBLE);
    
    return RET_OK(NULL);
}




/**
 * @brief Attempt to deserialize a json stream into given type.
 * @returns Result, either the data or an error.
 * @attention non-homogeneus arrays are not allowed for now.
 */
DeserializeResult deserialize_json(RStruct* model, void* stream) {
    if (!model) return RET_ERR(DE_NULL_PTR);
    json_walker jw = { .cur = 0, .eof = strlen((char*)stream) };
    size_t brk = arena_get_brk();
    jw.buff = stream;
    // Data structures
    struct _json_nesting_stack jss = {.stack = {0}, .overflown = 0, .sp = 0};
    unordered_buffer uob = _uob_init(0, model->total_size);

    DeserializeResult ret = { 0 };

    void* returned_instance = NULL;
    
    while(jw.cur < jw.eof)
    {
        char c = jw.buff[jw.cur];
        if (skip_chars(c)) { jw.cur++; continue; }
        if (c == '{') {
            _jss_push(&jss, 0);
            jw.cur++;
            ret = _json_dsobj(model, &jw, &jss, 0, &uob);
            break;
        } else {
            arena_set_brk(brk);
            return RET_ERR(DE_STREAM_INCOMPATIBLE);
        }
        jw.cur++;
    }
    // Trailing stuff
    while(skip_chars(jw.buff[jw.cur])) jw.cur++;
    
    
    if (ret.st == DR_OK) {
        // Expect EOF
        if (jw.cur != jw.eof - 1) {
            arena_set_brk(brk);
            return RET_ERR(DE_EXPECTED_EOF);
        }

        returned_instance = malloc(model->total_size);
        memcpy(returned_instance, uob.data, model->total_size);
        ret.data.buff = returned_instance;
        
    }


    arena_set_brk(brk);
    return ret;
}
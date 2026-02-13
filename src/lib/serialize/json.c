#define _GNU_SOURCE
#define _BSD_SOURCE 1
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include <crefl/types.h>
#include <crefl/cmp.h>

#include <serial/_common.h>
#include <serial/err.h>



char* deserialize_strerror(DeserializeError e);

#define MIN(a, b) (a > b)? b : a
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

typedef struct {
    uint8_t* buff;
    size_t cur;
    size_t eof;
} json_walker;

typedef enum {
    JSON_INVALID,
    JSON_OBJ_START,
    JSON_OBJ_END,
    JSON_KEY,
    JSON_INT,
    JSON_FLOAT,
    JSON_BOOLEAN,
    JSON_STRING,
    JSON_ARRAY_START,
    JSON_ARRAY_END,
} JSON_Tokens;

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


/**
 * @brief State machine deserialization of a JSON object.
 */
static DeserializeResult __ds_obj(
    RStruct* st, 
    json_walker* jw, 
    struct _json_nesting_stack* jss, 
    unordered_buffer* instance,
    size_t default_offset
) {
    temp_field_pool fp = _fp_init(FIELD_POOL_INITIAL);
    size_t substr_start=(size_t)-1;
    int scaped=0;           // scaping chars like \" \n \t
    int expecting_more=0;   // last field ended with ',' -> we expect more fields
    size_t curr_offset = 0; // the instance and type we are building are going to be packed.
    size_t n_elements=0;
    DeserializeError de;
    while (jw->cur < jw->eof)
    {
        RField curr = {0};
        curr.offset = curr_offset;
        if (skip_chars(currchar)) continue;
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
            if (!isalnum(currchar)) { 
                de = DE_UNEXPECTED_CHARACTER;
                goto return_err;
            }
            jw->cur++; goto reading_key;
        }

reading_value:
        while (skip_chars(currchar)) jw->cur++;
        if (currchar != ':') { 
            de = DE_UNEXPECTED_CHARACTER; goto return_err;}
        switch (currchar)
        {
        case '\"':
            substr_start = ++jw->cur;
            goto reading_string;
        
        /** @todo: rest of field kinds */

        default:
            break;
        }

reading_string:
        curr.kind = K_STRING;
        curr.size = 1;
        while (jw->cur < jw->eof) {
            if (currchar == '\"' && scaped == 0) {
                currchar = '\0';
                // Write pointer
                uint8_t* str = jw->buff + substr_start;
                _uob_append(instance, &str, sizeof(char*));
                goto finish_field;
            }
            // Actual scaping of characters will be handled later down the line.
            if (scaped == 1) scaped = 0;      // Only lasts one character   
            if (currchar == '\\') scaped ^= 1; // flip flag
            
            jw->cur++;
        }
        curr_offset += sizeof(char*);
        curr.next_offset = curr_offset;
        // Unused subtype


carve_key:
        currchar = '\0';
        curr.name = (const char*)jw->buff + substr_start;
        jw->cur++;
        substr_start = (size_t)-1;
        goto reading_value;

finish_field:
        // Skip until ending ','
        while (skip_chars(currchar)) jw->cur++;
        if (currchar == '}') {
            continue;
        }
        else if (currchar != ',') { 
            de = DE_UNEXPECTED_CHARACTER;
            goto return_err;
        }
        _fp_append(&fp, &curr);
        expecting_more = 1;
        jw->cur++;
        n_elements++;
        continue;

return_err:
        _fp_free(&fp);
        return RET_ERR(de);
    }

    // Final syntax checks
    if (jss->overflown != 0) {
        _fp_free(&fp);
        return RET_ERR(DE_TOO_MUCH_NESTING);
    }
    if (jss->sp != 0) { 
        _fp_free(&fp);
        return RET_ERR(DE_UNBALANCED_STREAM);
    }

    // Make the struct
    RStruct generated = {
        .n_elements = n_elements,
        .name_type = "placeholder",
        .total_size = curr_offset,
        .fields = calloc(sizeof(RField*), fp.sz)
    };
    for (size_t i = 0; i < n_elements; i++)
    {   
        generated.fields[i] = fp.f + i;
    }

    // Check compatibilty and migrate
    if (refl_comp_struct_schema(&generated, st) == 0) {
        _fp_free(&fp);
        return RET_ERR(DE_STREAM_INCOMPATIBLE);
    }
    uint8_t* result = refl_struct_migrate(&generated, st, instance->data);
    _fp_free(&fp);
    return RET_OK(result);
}

/**
 * @brief Attempt to deserialize a json stream into given type.
 * @returns Result, either the data or an error.
 * @attention non-homogeneus arrays are not allowed for now.
 * @attention stream will be mutated as a side effect of deserialization.
 */
DeserializeResult deserialize_json(RStruct* model, void* stream) {
    if (!model) RET_ERR(DE_NULL_PTR);
    json_walker jw = { .buff = stream, .cur = 0, .eof = strlen((char*)stream) };
    struct _json_nesting_stack jss = {.stack = {0}, .overflown = 0, .sp = 0};

    DeserializeResult ret = { 0 };
    unordered_buffer instance = _uob_init(INSTANCE_INITIAL);
    while(jw.cur < jw.eof)
    {
        char c = jw.buff[jw.cur];
        if (skip_chars(c)) { jw.cur++; continue; }
        if (c == '{') {
            _jss_push(&jss, 0);
            ret = __ds_obj(model, &jw, &jss, &instance, 0);
        } else {
            RET_ERR(DE_STREAM_INCOMPATIBLE);
        }
        jw.cur++;
    }
    _uob_free(&instance);
    return ret;
}
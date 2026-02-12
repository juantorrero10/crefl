#define _GNU_SOURCE
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <serial/generic_walker.h>
#include <serial/err.h>


#define MIN(a, b) (a > b)? b : a
#define MIN3(a, b, c) MIN(MIN(a, b), c)

#define not_allowed(_char) (_char == '\n' || _char == ' ' || _char == '\t')

#define RET_ERR(_err) (DeserializeResult){.st = DR_ERR, .data.err = _err}
#define RET_OK(_ptr) (DeserializeResult){.st = DR_OK, .data.buff = _ptr}


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


static inline void preprocess_json(json_walker* jw) {
    uint8_t* copy = calloc(1, jw->eof + 1);
    size_t copy_cur = 0;
    size_t new_eof = jw->eof;
    for (size_t i = 0; i < jw->eof; i++)
    {
        if (not_allowed(jw->buff[i])) { new_eof--; continue; }
        copy[copy_cur++] = jw->buff[i];
    }
    jw->eof = new_eof;
    jw->buff = copy;
}


DeserializeResult deserialize_json(RStruct* model, void* stream) {
    (void)model;
    json_walker jw = { .buff = stream, .cur = 0, .eof = strlen((char*)stream) };
    preprocess_json(&jw);
    puts((const char*)jw.buff);
    free(jw.buff);

    return RET_OK(jw.buff);
}
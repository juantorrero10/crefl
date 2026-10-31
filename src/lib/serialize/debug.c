#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <public/types.h>
#include <public/core.h>

#include <private/serial/common.h>

void dbg_print(RStruct* s, RObject instance, int __pretty) {
    SerializePolicy sp = {
        .arr_end = "]",
        .arr_start = "[",
        .arr_sep = ", ",
        .enum_as_string = 1,
        .field_sep = ", ",
        .format_primitive = generic_numeric_to_str,
        .indent_str = "    ",
        .key_end = NULL,
        .key_start = NULL,
        .scape_keys = 0,
        .key_val_sep = " = ",
        .newline = "\n",
        .obj_end = "}",
        .obj_start = "{",
        .opt_none = "None",
        .opt_some_start = "Some(",
        .opt_some_end = ")",
        .unwrap_optionals = 0,
        .pretty = __pretty,
        .str_end = "\"",
        .str_start = "\"",
        .null_ptr = "0x0",
        .null_str = "(null)",
        .scape_special_ascii = 1
    };
    char* out;
    size_t _sz = walk_type(&sp, s, instance, (void**)&out);
    printf("%.*s\n", (int)_sz, out);
    free(out);
}
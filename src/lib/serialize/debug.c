#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <crefl/types.h>
#include <crefl/core.h>

#include <crefl/serial/_common.h>

void debug_print_struct(RStruct* s, void* instance, int __pretty) {
    SerializePolicy sp = {
        .arr_end = "]",
        .arr_start = "[",
        .arr_sep = ", ",
        .enum_as_string = 1,
        .field_sep = ", ",
        .format_primitive = generic_numerical_converter,
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
        .str_start = "\""
    };
    char* out = (char*)walk_type(&sp, s, instance);
    printf("%s\n", out);
    free(out);
}
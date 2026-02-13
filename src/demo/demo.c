#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <crefl/core.h>
#include <crefl/types.h>
#include <crefl/cmp.h>
#include <crefl/patch.h>
#include <serial/formats.h>
#include <stdbool.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-function"


REFLECT(struct) {
    const char* string;
} ds_test_t;

int main() {
    refl_engine_load(CREFL_FILE);
    RStruct* t = refl_fetch_struct("ds_test_t");
    ds_test_t d = {.string = "Some string"};
    char* buff = serialize_json(t, &d);
    puts(buff);
    DeserializeResult dr = deserialize_json(t, buff);
    if (dr.st == DR_ERR) {
        printf("Couldn't deserialize: %s\n", dr.data.err_str);
    } else {
        ds_test_t* instance = (ds_test_t*)dr.data.buff;
        debug_print_struct(t, instance, 1);
        refl_generic_destructor(t, instance);
    }

    free(buff);
    return 0;
}

#pragma GCC diagnostic pop

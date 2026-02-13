#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <crefl/core.h>
#include <crefl/types.h>
#include <crefl/cmp.h>
#include <crefl/patch.h>
#include <serial/formats.h>
#include <stdbool.h>

#define JSON_STREAM \
"{"\
"    \"string\" : \"Some string\","\
"    \"id\" : 400000000,"\
"    \"f\" : 3.5"\
"}"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-function"


REFLECT(struct) {
    const char* string;
    uint32_t id;
    float f;
} ds_test_t;

int main() {
    refl_engine_load(CREFL_FILE);
    RStruct* t = refl_fetch_struct("ds_test_t");
    DeserializeResult dr = deserialize_json(t, JSON_STREAM);
    if (dr.st == DR_ERR) {
        printf("Couldn't deserialize: %s\n", dr.data.err_str);
    } else {
        ds_test_t* instance = (ds_test_t*)dr.data.buff;
        debug_print_struct(t, instance, 1);
        refl_generic_destructor(t, instance);
    }

    return 0;
}

#pragma GCC diagnostic pop

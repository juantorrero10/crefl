#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <crefl/core.h>
#include <crefl/types.h>
#include <crefl/cmp.h>
#include <crefl/patch.h>
#include <serial/formats.h>
#include <_arena.h>
#include <stdbool.h>

#define JSON_STREAM "test.json"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-function"

REFLECT(struct) {
    uint32_t a;
    OPTION(uint32_t) b;
} __P;

REFLECT(struct) {
    uint32_t i;
    __P field;    
} ds_test_t;

int main() {
    refl_engine_load(CREFL_FILE);
    RStruct* t = refl_fetch_struct("ds_test_t");

    FILE* f = fopen(JSON_STREAM, "r");
    fseek(f, 0, SEEK_END);
    int64_t size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* stream = arena_alloc(0, size);
    fread(stream, sizeof(char), size, f);


    DeserializeResult dr = deserialize_json(t, stream);
    if (dr.st == DR_ERR) {
        printf("Couldn't deserialize: %s\n", dr.data.err_str);
    } else {
        ds_test_t* instance = (ds_test_t*)dr.data.buff;
        debug_print_struct(t, instance, 1);
        refl_generic_destructor(t, instance);
    }
    fclose(f);
    return 0;
}

#pragma GCC diagnostic pop

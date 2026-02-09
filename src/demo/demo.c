#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../include/public/core.h"
#include "../include/public/types.h"
#include "../include/public/cmp.h"
#include "../include/public/patch.h"
#include <serial/debug.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-function"


REFLECT(struct) {
    struct a_ {
        uint32_t a;
        uint32_t b;
    } nested_shit;
    uint64_t numbers[4];
    uint32_t id;
    const char* name;
} case1;

REFLECT(struct) {
    uint64_t numbers[4];
    const char* name;
    uint32_t id;
    double fl;
    uint32_t* ptr;
} case2;

REFLECT(struct) {
    uint32_t a;
    uint32_t b;
} __annnn;

REFLECT(struct) {
    OPTION(__annnn) nested_shit;
    uint64_t numbers[4];
    OPTION(const char*) name;
    uint64_t id;
    OPTION(double) fl;
} model;


int main() {
    refl_engine_load(CREFL_FILE);
    case1 a = { .nested_shit = { 2, 3},  .name = "XxJamoncitoxX" }; (void)a;
    case2 b = { .name = "Some Name", .fl = 4.5f, .ptr = (uint32_t*)&a, .numbers = {2, 3, 4, 4} };
    RStruct* c1 = refl_fetch_struct("case1"); (void)c1;
    RStruct* c2 = refl_fetch_struct("case2"); (void)c2;
    RStruct* mod = refl_fetch_struct("model");

    if (refl_comp_struct_schema(mod, c1)) {
        model* deserialized = refl_struct_migrate(c1, mod, &a);
        (void)deserialized;
        debug_print_struct(mod, deserialized, 1);

    } else {
        printf("Not compatible!!\n");
    }



    return 0;
}

#pragma GCC diagnostic pop

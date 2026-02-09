#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../include/public/core.h"
#include "../include/public/types.h"
#include "../include/public/cmp.h"
#include "../include/public/patch.h"
#include <serial/debug.h>
#include <stdbool.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-function"

REFLECT(struct) {
    uint32_t name;
    uint32_t id;
} gen_t;

REFLECT(struct) {
    OPTION(uint64_t) name;
    uint32_t id;
} patchable_t;



int main() {
    refl_engine_load(CREFL_FILE);

    gen_t user = {
        .id = 1,
        .name = 2,
    };
    
    RStruct* m = refl_fetch_struct("patchable_t");
    RStruct* g = refl_fetch_struct("gen_t");
    
    debug_print_struct(g, &user, 1);

    patchable_t* deser = refl_struct_migrate(g, m, &user);

    debug_print_struct(m, deser, 1);

   //ree(deser->name.array);
    free(deser);
    //printf("%p\n", user.name);


    

    return 0;
}

#pragma GCC diagnostic pop

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
    bool can_ban;
    bool can_play;
} permisions_t;

REFLECT(struct) {
    OPTION(const char*) name;
    permisions_t perms;
    uint32_t id;
} patchable_t;



int main() {
    refl_engine_load(CREFL_FILE);

    patchable_t p = {
        .name = {SOME, "fucking name"},
        .perms = {
            .can_ban = 0,
            .can_play = 1,
        },
        .id = 1
    };

    RStruct* pa = refl_fetch_struct("patchable_t");

    size_t tot = pa->total_size;
    size_t disp;
    for (size_t i = 0; i < pa->n_elements; i++)
    {
        disp = (i == pa->n_elements - 1)? pa->total_size : pa->fields[i]->next_offset;
        printf("%zu -> %zu\n", pa->fields[i]->offset, disp);
    }
    

    debug_print_struct(pa,  &p, 1);
    

    return 0;
}

#pragma GCC diagnostic pop

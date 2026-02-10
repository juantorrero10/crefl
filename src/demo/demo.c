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
    uint32_t a;
    uint32_t b;
} og_t;

REFLECT(struct) {
    uint64_t c;
    uint32_t a;
    uint32_t b;
} new_t;

REFLECT(struct) {
    bool can_play;
    bool can_ban;
} perms_t;

REFLECT(struct) {
    OPTION(const char*) name;
    OPTION(perms_t) perms;
    uint32_t id;
} userr_t;

int main() {
    refl_engine_load(CREFL_FILE);
    RStruct* t1 = refl_fetch_struct("og_t");
    RStruct* t2 = refl_fetch_struct("new_t");
    RStruct* t3 = refl_fetch_struct("perms_t");
    RStruct* t4 = refl_fetch_struct("userr_t");
    

    return 0;
}

#pragma GCC diagnostic pop

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
    bool can_ban;
    bool can_play;
} perms_t;

REFLECT(struct) {
    DYN_ARRAY(uint32_t) ids;
    OPTION(const char*) name;
    OPTION(perms_t) perms;
} userr_t;

int main() {
    refl_engine_load(CREFL_FILE);
    RStruct* t4 = refl_fetch_struct("userr_t");

    RField change_field = *(refl_field_by_name(t4, "ids"));
    change_field.kind = K_FIXED_ARRAY; change_field.size = 4;
    RField f2 = *(refl_field_by_name(t4, "perms")->subtype.subfield);
    RPatch patches[] = {
        { .action = RPA_CHANGE_FIELD, .action_data.field = &change_field, .target = "ids"},
        { .action = RPA_CHANGE_FIELD, .action_data.field = &f2, .target = "perms"},
        { .action = RPA_RENAME_FIELD, .action_data.renamed = "perms", .target = "optional"},
    };
    perms_t p = {.can_ban = 0, .can_play = 1};
    userr_t old_inst = { 
        .ids = {.array = (uint32_t[]){1, 2, 3, 4, 5}, .chunk_size = sizeof(uint32_t), .size = 5, .capacity = 5}, 
        .name = {SOME, "JuanchoPancho"}, 
        .perms = {.opt=SOME, .optional=&p} };

    debug_print_struct(t4, &old_inst, 1);

    RStruct* new = refl_patch_struct(t4, patches, 3);
    

    void* new_instance = refl_struct_migrate(t4, new, &old_inst);

    perms_t* ppp = refl_get_field(new, "perms", &new_instance);

    debug_print_struct(new, new_instance, 1);

    refl_destroy_struct(new);

    return 0;
}

#pragma GCC diagnostic pop

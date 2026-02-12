#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../include/public/core.h"
#include "../include/public/types.h"
#include "../include/public/cmp.h"
#include "../include/public/patch.h"
#include <serial/formats.h>
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

REFLECT(enum) {
    VALUE_1,
    VALUE_2,
    OKOKOK,
    SOME_FUCKING_EROR
} some_enum;

REFLECT(struct) {
    OPTION(const char*) name;
    DYN_ARRAY(uint32_t) ids;
    OPTION(uint64_t) nothing_here;
    some_enum enum_variable;
    OPTION(perms_t) perms;
} userr_t;

int main() {
    refl_engine_load(CREFL_FILE);
    RStruct* t4 = refl_fetch_struct("userr_t");
    perms_t p = {.can_ban = 0, .can_play = 1};
    userr_t old_inst = { 
        .ids = {.array = (uint32_t[]){1, 2, 3, 4, 5}, .chunk_size = sizeof(uint32_t), .size = 5, .capacity = 5}, 
        .name = {SOME, "JuanchoPancho"}, 
        .perms = {.opt=SOME, .optional=&p},
        .enum_variable = SOME_FUCKING_EROR
    };

    void* buff = serialize_json(t4, &old_inst);
    printf("%s\n", (char*)buff);
    DeserializeResult inst = deserialize_json(t4, buff);
    if (inst.st == DR_ERR) {
        printf("Could not deserialize: '%s'\n", deserialize_strerror(inst.data.err));
    } else {
        printf("Returned pointer: %p\n", inst.data.buff);
    }
    free(buff);

    return 0;
}

#pragma GCC diagnostic pop

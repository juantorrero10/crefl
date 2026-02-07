#ifndef TYPE_H_
#define TYPE_H_


#include "../include/public/types.h"

REFLECT(struct) {
    uint32_t id;
    OPTION(const char*) username;
    struct name
    {
        uint32_t a;
        uint32_t b;
    } some;
    DYN_ARRAY(uint32_t) indices;
    uint8_t final;
    void* wrongly_ptr;
} data;

typedef struct {
    void* a;
    void* b;
} pair_t;

REFLECT(enum) {
    T_OK = 10,
    T_ERR = 20,
} result_t;

typedef enum {
    T_SOME,
    T_NONE,
} option_t;

REFLECT(union)
{
    uint64_t a;
    bool b;
    result_t res;
    OPTION(const char*) string;
} some_union;


REFLECT(struct) {
    uint32_t id;
    const char* name;
    uint8_t** fuck;
} superior;


REFLECT(struct) {
    int32_t id;
    OPTION(int32_t) id_superior;
    const char* name;
} user_t;


#endif // TYPE_H_
#ifndef TYPE_H_
#define TYPE_H_


#include <refl_types.h>

REFLECT(struct) {
    uint32_t id;
    OPTION(const char*) username;
    inner_type some;
    DYN_ARRAY(int) indices;
    uint8_t final;
    void* wrongly_ptr;
} data;

typedef struct {
    void* a;
    void* b;
} pair_t;

REFLECT(enum) {
    T_OK,
    T_ERR,
} result_t;

typedef enum {
    T_SOME,
    T_NONE,
} option_t;

REFLECT(union)
{
    uint64_t a;
    bool b;
    OPTION(const char*) string;
} some_union;


REFLECT(struct) {
    int id;
    const char* name;
} superior;


REFLECT(struct) {
    int id;
    OPTION(int) id_superior;
    const char* name;
} user_t;


#endif // TYPE_H_
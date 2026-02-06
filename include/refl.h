#ifndef REFL_H_
#define REFL_H_


#include <refl_types.h>

typedef struct {
    const char* k_name;
    RStruct val;
} refl_st_entry_t;

typedef struct {
    refl_st_entry_t* map;
    size_t count;
    size_t capacity;
} refl_st_hashmap_t;



extern refl_st_hashmap_t G_REFL_STRUCTS;
extern size_t G_REFL_STRUCT_AMOUNT;
extern REnum* G_REFL_ENUMS;
extern size_t G_REFL_ENUMS_AMOUNT;
extern char* G_STRING_POOL;



RStruct* refl_fetch_struct(const char* type_name);
RObject refl_fetch_from_instance(RStruct* refl_struct, int field_index, RObject instance);





#endif // REFL_H_
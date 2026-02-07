#ifndef REFL_H_
#define REFL_H_


#include "types.h"

typedef struct {
    const char* k_name;
    RStruct val;
} refl_st_entry_t;

typedef struct {
    refl_st_entry_t* map;
    size_t count;
    size_t capacity;
} refl_st_hashmap_t;

typedef struct {
    const char* k_name;
    REnum val;
} refl_en_entry_t;

typedef struct {
    refl_en_entry_t* map;
    size_t count;
    size_t capacity;
} refl_en_hashmap_t;

typedef struct {
    const char* k_name;
    RUnion val;
} refl_un_entry_t;

typedef struct {
    refl_un_entry_t* map;
    size_t count;
    size_t capacity;
} refl_un_hashmap_t;



extern refl_st_hashmap_t G_REFL_STRUCTS;
extern refl_en_hashmap_t G_REFL_ENUMS;
extern refl_un_hashmap_t G_REFL_UNIONS;
extern char* G_STRING_POOL;


void refl_engine_load(const char* file_path);


RStruct* refl_fetch_struct(const char* name);
RObject refl_fetch_from_instance(RStruct* refl_struct, size_t field_index, RObject instance);





#endif // REFL_H_
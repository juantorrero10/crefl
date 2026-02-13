#ifndef FETCHED_H_
#define FETCHED_H_

#include <crefl/types.h>

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

typedef struct {
    RField* fields;
    size_t count;
    size_t capacity;
} refl_fi_array_t;    



extern refl_st_hashmap_t G_REFL_STRUCTS;
extern refl_en_hashmap_t G_REFL_ENUMS;
extern refl_un_hashmap_t G_REFL_UNIONS;
extern refl_fi_array_t G_REFL_FIELDS;
extern char* G_STRING_POOL;

#endif // FETCHED_H_
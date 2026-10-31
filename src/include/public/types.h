#ifndef REFL_TYPES_H_
#define REFL_TYPES_H_

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _WIN32
typedef signed char i8;
typedef short i16;
typedef int i32;
typedef long i64;
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long u64;
#else
typedef signed char i8;
typedef short i16;
typedef int i32;
typedef long long i64;
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;
#endif


#define REFLECT(type) typedef type
//#define OPTION(type) r_option_t
#define DYN_ARRAY(type) r_array_t 

#define DECL_OPTION(type) typedef struct { bool is_some; type data; } __OPTION_##type##_DECL__
#define DECL_OPTION_DYN_ARRAY(type) typedef struct { bool is_some; r_array_t arr; } __OPTION_DA_##type##_DECL__
#define DECL_OPTION_FIXED_ARRAY(type, N) typedef struct { bool is_some; type arr[N]; } __OPTION_FA_##type##_##N##_DECL__
#define OPTION(type) __OPTION_##type##_DECL__
#define OPTION_DYN_ARRAY(type) __OPTION_DA_##type##_DECL__
#define OPTION_FIXED_ARRAY(type, N) __OPTION_FA_##type##_##N##_DECL__


REFLECT(struct) {
    char* data;
} strZ;

DECL_OPTION(strZ);
DECL_OPTION(bool);
DECL_OPTION(u8);
DECL_OPTION(i8);
DECL_OPTION(u16);
DECL_OPTION(i16);
DECL_OPTION(u32);
DECL_OPTION(i32);
DECL_OPTION(u64);
DECL_OPTION(i64);
DECL_OPTION(float);
DECL_OPTION(double);


REFLECT(enum) {
    K_PRIMITIVE,
    K_STRING,
    K_POINTER,
    K_STRUCT,
    K_UNION,
    K_ENUM,
    K_DYN_ARRAY,
    K_FIXED_ARRAY,
    K_OPTION
} RKind;

REFLECT(enum) {
    P_U8,  P_I8,
    P_U16, P_I16,
    P_U32, P_I32,
    P_U64, P_I64,
    P_F32, P_F64,
    P_BOOL,
    P_U0  // aka. void
} RPrimitive;

typedef struct _CREFL_FIELD_ RField;

typedef struct {
    const char* name_type;
    RField** fields;
    size_t n_elements;
    size_t total_size;
} RStruct, RUnion;

typedef struct { 
    const char* name_type;
    const char** names;
    i64* indexes;
    size_t n_elements;
} REnum;

typedef struct _CREFL_FIELD_{
    const char* name;
    size_t offset;
    size_t next_offset;
    size_t size;          // Array count, except K_OPTION stores the wrapper byte size
    RKind kind;
    union {
        RPrimitive prim;
        RField* subfield; // For pointer, option, fixed and dyn arrays
        REnum* e;
        RStruct* s;
        RUnion* u;
    } subtype;
} RField;

static inline bool refl_option_is_some(const void* option_value) {
    return *(const bool*)option_value;
}

static inline void* refl_option_data(const RField* option_field, void* option_value) {
    return (u8*)option_value + option_field->subtype.subfield->offset;
}

typedef void* RObject;

typedef struct {
    RStruct* type;
    void* instance;
} _RObject;

// Legacy heap-backed optional representation; OPTION(type) uses inline wrappers.
typedef struct _CREFL_OPTION_ {
    enum opt_states { NONE, SOME } opt;
    void* optional;
    size_t size;
} r_option_t;

typedef struct _CREFL_DYN_ARRAY {
    void* array;
    size_t size;
    size_t capacity;
    size_t chunk_size;
} r_array_t;


#ifdef __cplusplus
}
#endif

#endif // REFL_TYPES_H_
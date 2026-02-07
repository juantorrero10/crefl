#ifndef REFL_TYPES_H_
#define REFL_TYPES_H_

#include <stdint.h>
#include <stddef.h>


typedef enum {
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

typedef enum {
    P_U8,  P_I8,
    P_U16, P_I16,
    P_U32, P_I32,
    P_U64, P_I64,
    P_F32, P_F64,
    P_BOOL,
    P_U0  // aka. void
} KPrimitive;

typedef struct _CREFL_FIELD_ RField;

typedef struct {
    const char* name_type;
    RField* fields;
    size_t n_elements;
} RStruct, RUnion;

typedef struct { 
    const char* name_type;
    const char** names;
    size_t* indexes;
    size_t n_elements;
} REnum;

typedef struct _CREFL_FIELD_{
    const char* name;
    size_t offset;
    size_t size;          // For dyn arrays, always 1 for other kinds 
    RKind kind;
    union {
        KPrimitive prim;
        RField* subfield; // For pointer, option, fixed and dyn arrays
        REnum* e;
        RStruct* s;
        RUnion* u;
    } subtype;
} RField;

typedef void* RObject;


// Data wrappers for defining uses cases for a pointer
// These will be generated into K_OPTION and K_DYN_ARRAY
typedef struct _CREFL_OPTION_ {
    enum { SOME, NONE } opt;
    void* data;
    size_t size;
} r_option_t;

typedef struct _CREFL_DYN_ARRAY {
    void* data;
    size_t size;
    size_t capacity;
    size_t chunk_size;
} r_array_t;




#define REFLECT(type) typedef type
#define OPTION(type) r_option_t
#define DYN_ARRAY(type) r_array_t 

REFLECT(struct) {
    uint64_t data;
    uint64_t another_data;
} inner_type;



#endif // REFL_TYPES_H_
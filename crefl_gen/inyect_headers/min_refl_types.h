#ifndef MIN_REFL_TYPES_H_
#define MIN_REFL_TYPES_H_

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

#endif // MIN_REFL_TYPES_H_
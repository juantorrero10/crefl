#ifndef MIN_REFL_TYPES_H_
#define MIN_REFL_TYPES_H_
#define REFLECT(type) typedef type
#define OPTION(type) r_option_t
#define DYN_ARRAY(type) r_array_t

REFLECT(enum) {
    NONE,
    SOME
} opt_states;

REFLECT(struct){
    opt_states opt;
    void* optional;
    size_t size;
} r_option_t;

typedef struct _CREFL_DYN_ARRAY {
    void* data;
    size_t size;
    size_t capacity;
    size_t chunk_size;
} r_array_t;



#endif // MIN_REFL_TYPES_H_
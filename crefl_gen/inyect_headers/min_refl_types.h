#ifndef MIN_REFL_TYPES_H_
#define MIN_REFL_TYPES_H_
#define REFLECT(type) typedef type
#define DECL_OPTION(type) typedef struct { bool is_some; type data; } __OPTION_##type##_DECL__
#define DECL_OPTION_DYN_ARRAY(type) typedef struct { bool is_some; r_array_t arr; } __OPTION_DA_##type##_DECL__
#define DECL_OPTION_FIXED_ARRAY(type, N) typedef struct { bool is_some; type arr[N]; } __OPTION_FA_##type##_##N##_DECL__
#define OPTION(type) __OPTION_##type##_DECL__
#define OPTION_DYN_ARRAY(type) __OPTION_DA_##type##_DECL__
#define OPTION_FIXED_ARRAY(type, N) __OPTION_FA_##type##_##N##_DECL__
#define DYN_ARRAY(type) r_array_t

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
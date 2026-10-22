#ifndef SERIAL_GENERIC_WALKER_H_
#define SERIAL_GENERIC_WALKER_H_

#include <crefl/types.h>



typedef struct {

    const char* obj_start;
    const char* obj_end;

    const char* field_sep;        // between fields
    const char* key_val_sep;      // between key and value

    const char* arr_start;
    const char* arr_end;
    const char* arr_sep;

    const char* str_start;
    const char* str_end;
    const char* null_str;

    const char* null_ptr;

    int unwrap_optionals;           // Some("String") -> "String", None -> <Skip field>
    const char* opt_some_start;
    const char* opt_some_end;
    const char* opt_none;


    int enum_as_string;   // 1 = label, 0 = numeric

    int scape_keys;
    const char* key_start;
    const char* key_end;

    int pretty;
    const char* indent_str;
    const char* newline;

    int scape_special_ascii;


    void (*format_primitive)(
        RPrimitive p,
        uint64_t raw,
        void* buff,
        size_t sz
    );

} SerializePolicy;

typedef struct {
    RField* f;
    size_t sz;
    int n_arena;
    size_t capacity;
} temp_field_pool;

typedef struct {
    void* data;
    size_t chunk_size;
    size_t size;
    int n_arena;
    size_t capacity;
} g_arraylist_t;

typedef struct {
    uint8_t* data;
    size_t cur;
    int n_arena;
    size_t capacity;
} unordered_buffer;

typedef struct {
    uint8_t* buff;
    size_t cur;
    size_t eof;
} stream_walker;

int generic_str_to_numeric(
    stream_walker* sw,
    uint64_t* out_bytes,
    RPrimitive* out_prim_kind
);

temp_field_pool _fp_init(int n_arena, size_t initial);
void _fp_append(temp_field_pool* fp, RField* f);
void _fp_free(temp_field_pool* fp);
g_arraylist_t _gal_init(int n_arena, size_t initial, size_t chunk_size);
void _gal_append(g_arraylist_t* gal, void* object);
void _gal_free(g_arraylist_t* gal);
void _gal_empty(g_arraylist_t* gal);
unordered_buffer _uob_init(int n_arena, size_t initial);
void _uob_append(unordered_buffer* uob, void* src, size_t sz); 
void _uob_free(unordered_buffer* uob);
void _uob_expand(unordered_buffer* uob, size_t sz);

typedef union {
    void* ptr;
    char* str;
    char c;
    uint64_t u;
    int64_t s;
    double f;
} any_t;

#define RET_ERR(_err) (DeserializeResult){.st = DR_ERR, .err = _err, .data.err_str=deserialize_strerror(_err)}
#define RET_OK(_ptr) (DeserializeResult){.st = DR_OK, .data.buff = _ptr}


void generic_numeric_to_str(RPrimitive p, uint64_t bytes, void* buff, size_t sz);
size_t walk_type(SerializePolicy* sp, RStruct* s, RObject instance, void** out);


#endif // SERIAL_GENERIC_WALKER_H_
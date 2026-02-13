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
    size_t capacity;
} temp_field_pool;

typedef struct {
    void* data;
    size_t chunk_size;
    size_t size;
    size_t capacity;
} g_arraylist_t;

typedef struct {
    uint8_t* data;
    size_t cur;
    size_t capacity;
} unordered_buffer;

temp_field_pool _fp_init(size_t initial);
void _fp_append(temp_field_pool* fp, RField* f);
void _fp_free(temp_field_pool* fp);
g_arraylist_t _gal_init(size_t initial, size_t chunk_size);
void _gal_append(g_arraylist_t* gal, void* object);
void _gal_free(g_arraylist_t* gal);
unordered_buffer _uob_init(size_t initial);
void _uob_append(unordered_buffer* uob, void* src, size_t sz); 
void _uob_free(unordered_buffer* uob);

typedef union {
    void* ptr;
    char* str;
    char c;
    uint64_t u;
    int64_t s;
    double f;
} any_t;


void generic_numerical_converter(RPrimitive p, uint64_t bytes, void* buff, size_t sz);

void* walk_type(SerializePolicy* sp, RStruct* s, void* instance);


#endif // SERIAL_GENERIC_WALKER_H_
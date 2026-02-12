#ifndef SERIAL_GENERIC_WALKER_H_
#define SERIAL_GENERIC_WALKER_H_

#include <public/types.h>

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

void generic_numerical_converter(RPrimitive p, uint64_t bytes, void* buff, size_t sz);

void* walk_type(SerializePolicy* sp, RStruct* s, void* instance);


#endif // SERIAL_GENERIC_WALKER_H_
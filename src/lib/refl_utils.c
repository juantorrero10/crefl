#include <string.h>

#include <crefl/core.h>
#include <crefl/types.h>
#include <stdbool.h>


const char* refl_enum_get_label(REnum* type, int val) {
    // Straight correlation
    if (val < (int)type->n_elements && (int)type->indexes[val] == val) return type->names[val];
    // Binary search
    int32_t low = 0;
    int32_t high = type->n_elements - 1;

    while (low <= high) {
        int32_t mid = low + (high - low) / 2;
        int val_at = type->indexes[mid];

        if (val_at == val) {
            return type->names[mid];
        } else if (val_at < val) {
            low = mid + 1;
        } else {
            high = mid - 1;
        }
    }

    return NULL; // not found
}

size_t refl_prim_to_size(RPrimitive p) {
    switch (p)
    {
    case P_U0:
        return 0;
    case P_BOOL:
        return sizeof(bool);
    case P_F32:
    case P_I32:
    case P_U32:
        return 4;
    case P_F64:
    case P_I64:
    case P_U64:
        return 8;
    case P_U16:
    case P_I16:
        return 2;
    case P_U8:
    case P_I8:
        return 1;
    
    default:
        break;
    }
    return sizeof(int);
}


RField* refl_field_by_name(RStruct* s, const char* name, int* opt_out_idx) {
    RField* f;
    for (size_t i = 0; i < s->n_elements; i++)
    {
        f = s->fields[i];
        if (strcmp(name, f->name) == 0) { 
            if (opt_out_idx != NULL) *opt_out_idx = i;
            return s->fields[i]; 
        }
    }
    if (opt_out_idx != NULL) *opt_out_idx = -1;
    return NULL;
}


RObject refl_get_field_value(RStruct* st, RObject instance, int idx) {
    return *(void**)((uint8_t*)instance + st->fields[idx]->offset);
}

size_t refl_get_field_size(RField* f) {
    switch (f->kind)
    {
    case K_PRIMITIVE:
        return refl_prim_to_size(f->subtype.prim);
    case K_POINTER:
    case K_STRING:
        return sizeof(char*);
    case K_UNION:
    case K_STRUCT:
        return f->subtype.s->total_size;
    case K_ENUM:
        return sizeof(int);
    case K_OPTION:
        return sizeof(r_option_t);
    case K_DYN_ARRAY:
        return sizeof(r_array_t);
    case K_FIXED_ARRAY:
        return refl_get_field_size(f->subtype.subfield) * f->size;
    }
    return 0;
}
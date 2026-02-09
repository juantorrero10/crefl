#include <string.h>

#include <public/core.h>
#include <public/types.h>


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
        return sizeof(int);
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
    return (size_t)-1;
}


RField* refl_field_by_name(RStruct* s, const char* name) {
    for (size_t i = 0; i < s->n_elements; i++)
    {
        if (strcmp(name, s->fields[i]->name) == 0) return s->fields[i];
    }
    return NULL;
}


RObject refl_get_field_value(RStruct* st, RObject instance, int idx) {
    return *(void**)((uint8_t*)instance + st->fields[idx]->offset);
}
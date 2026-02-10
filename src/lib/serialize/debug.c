#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <public/types.h>
#include <public/core.h>


static void _to_text_any_numerical(RPrimitive p, uint64_t bytes, void* buff, size_t sz) {
    switch (p)
    {
    case P_BOOL:
        bytes &= 0xFF;
        (bytes >= 1)
        ? snprintf(buff, sz, "true")
        : snprintf(buff, sz, "false");
        break;
    case P_U0:
        snprintf(buff, sz, "<no_val>");
        break;
    case P_U8:
        snprintf(buff, sz, "%hhu", (uint8_t)(bytes & 0xFF));
        break;
    case P_I8:
        snprintf(buff, sz, "%hhd", (int8_t)(bytes));
        break;
    case P_U16:
        snprintf(buff, sz, "%hu", (uint16_t)(bytes & 0xFFFF));
        break;
    case P_I16:
        snprintf(buff, sz, "%hd", (int16_t)(bytes));
        break;
    case P_U32:
        snprintf(buff, sz, "%u", (uint32_t)(bytes & 0xFFFFFFFF));
        break;
    case P_I32:
        snprintf(buff, sz, "%d", (int32_t)(bytes));
        break;
    case P_U64:
        snprintf(buff, sz, "%lu", (bytes));
        break;
    case P_I64:
        snprintf(buff, sz, "%ld", (int64_t)bytes);
        break;
    case P_F32:
        union { uint32_t u; float f; } v = { .u = (uint32_t)bytes };
        snprintf(buff, sz, "%f", v.f);
        break;
    case P_F64:
        union { uint64_t u; double d; } v2 = { .u = bytes };
        snprintf(buff, sz, "%lf", v2.d);
        break;
    default:
        break;
    }
}

typedef struct {
    char* buff;
    size_t size;
    size_t cap;
} char_vec;

static void str_push(char_vec* cv, const char* str) {
    if (str == NULL) {
        str_push(cv, ("(null)"));
        return;
    }
    size_t sz = strlen(str);

    while (cv->size + sz + 1 > cv->cap) {
        size_t old = cv->cap;
        cv->cap *= 2;
        cv->buff = realloc(cv->buff, cv->cap);
        memset(cv->buff + old, 0, cv->cap - old);
    }

    memcpy(cv->buff + cv->size, str, sz);
    cv->size += sz;
    cv->buff[cv->size] = '\0';
}


static void str_push_tabs(char_vec* cv, int amnt) {
    for (int i = 0; i < amnt; i++)
    {
        str_push(cv, "    ");
    }
}

static void load_numeric_bytes(RField* f, void* instance, uint64_t* out) {
    *out = 0UL;
    memcpy(out, (uint8_t*)instance + f->offset, refl_prim_to_size(f->subtype.prim));
}



static void __dbg_print(RStruct* s, void* instance, int p, int indent, char_vec* v, char* buff2);


static void __dbg_p_field(RField* f, void* instance, int p, int indent, char_vec* v, char* buff2) {
    r_option_t* o;
    r_array_t* a;
    switch (f->kind)
        {
        case K_PRIMITIVE:
            uint64_t raw;
            load_numeric_bytes(f, instance, &raw);
            _to_text_any_numerical(f->subtype.prim, raw, buff2, 50);
            str_push(v, buff2);
            break;

        case K_STRING:
            char* str = *(char**)((uint8_t*)instance + f->offset);
            if (str) str_push(v, "\"");
            str_push(v, str);
            if (str) str_push(v, "\"");
            break;
        case K_OPTION:
            o = (r_option_t*)((uint8_t*)instance + f->offset);
            if (o->opt == SOME) {
                str_push(v, "Some(");
                __dbg_p_field(f->subtype.subfield, (void*)&o->optional, p, indent, v, buff2);
                str_push(v, ")");
            } else {
                str_push(v, "None");
            }break;
        case K_DYN_ARRAY:
            a = (r_array_t*)((uint8_t*)instance + f->offset);
            str_push(v, "[ ");
            for (size_t i = 0; i < a->size; i++)
            {
                __dbg_p_field(f->subtype.subfield, (void*)((uint8_t*)a->array + (i * a->chunk_size)), p, indent, v, buff2);
                if (i != a->size - 1) str_push(v, ", ");
            }
            str_push(v, " ]");break;
        case K_FIXED_ARRAY:
            str_push(v, "[ ");

            for (size_t i = 0; i < f->size; i++)
            {
                __dbg_p_field(f->subtype.subfield, (uint8_t*)instance + f->offset + (i * refl_get_field_size(f->subtype.subfield)), p, indent, v, buff2);
                if (i != f->size - 1) str_push(v, ", ");
            }
            str_push(v, " ]");break;
        case K_ENUM:
            int val;
            memcpy(&val, (uint8_t*)instance + f->offset, sizeof(int));
            str_push(v, (char*)refl_enum_get_label(f->subtype.e, val));
            break;
        case K_STRUCT:
            __dbg_print(f->subtype.s, (uint8_t*)instance + f->offset, p, indent + 1, v, buff2);break;
        case K_POINTER:
            void* ptr = *(void**)((uint8_t*)instance + f->offset);
            snprintf(buff2, 50, "%p", ptr);
            str_push(v, buff2);
            break;
        case K_UNION:
            str_push(v, "<union>");break;
        default:
            break;
        }
        

}

static void __dbg_print(RStruct* s, void* instance, int p, int indent, char_vec* v, char* buff2) {
    (p)
    ?str_push(v, "{\n")
    :str_push(v, "{");
    indent++;
    for (size_t i = 0; i < s->n_elements; i++)
    {
        if (p) str_push_tabs(v,indent);
        RField* f = s->fields[i];
        str_push(v, (char*)f->name);
        str_push(v, " = ");
        __dbg_p_field(f, instance, p, indent, v, buff2); 
        if (i != s->n_elements - 1) {
            (p)
            ? str_push(v, ",\n")
            : str_push(v, ", ");
        }
    }
    indent--;
    str_push(v, "\n");
    if (p) str_push_tabs(v,indent);
    str_push(v, "}");
}


void debug_print_struct(RStruct* s, void* instance, int __pretty) {
    if (!s) return;
    char_vec v = { .buff = malloc(50), .cap = 50, .size = 0 };
    memset(v.buff, 0, 50);
    char buff[50];
    __dbg_print(s, instance, __pretty, 0, &v, buff);
    printf("%s\n", v.buff);
    free(v.buff);
}
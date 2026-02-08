#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../include/public/core.h"
#include "../include/public/types.h"


REFLECT(struct) {
    const char* name;
    const char* password;
} LoginPayload;

static void get_primitive(char *buf, size_t buf_sz, RPrimitive p, uint64_t val)
{
    switch (p)
    {
        case P_BOOL:
            snprintf(buf, buf_sz, "%s", val ? "true" : "false");
            break;

        case P_I8:
            snprintf(buf, buf_sz, "%d", (int8_t)val);
            break;

        case P_U8:
            snprintf(buf, buf_sz, "%u", (uint8_t)val);
            break;

        case P_I16:
            snprintf(buf, buf_sz, "%d", (int16_t)val);
            break;

        case P_U16:
            snprintf(buf, buf_sz, "%u", (uint16_t)val);
            break;

        case P_I32:
            snprintf(buf, buf_sz, "%d", (int32_t)val);
            break;

        case P_U32:
            snprintf(buf, buf_sz, "%u", (uint32_t)val);
            break;

        case P_I64:
            snprintf(buf, buf_sz, "%ld", (int64_t)val);
            break;

        case P_U64:
            snprintf(buf, buf_sz, "%lu", (uint64_t)val);
            break;

        case P_F32: {
            float f;
            memcpy(&f, &val, sizeof(float));
            snprintf(buf, buf_sz, "%g", f);
            break;
        }

        case P_F64: {
            double d;
            memcpy(&d, &val, sizeof(double));
            snprintf(buf, buf_sz, "%g", d);
            break;
        }

        default:
            snprintf(buf, buf_sz, "<unknown>");
            break;
    }
}



void serialize_json(const char* type, RObject instance, char* buff) {
    RStruct* st = refl_fetch_struct(type);
    if (!st) return;
    sprintf(buff, "{ ");
    char buff2[50];
    uint32_t curr = 2;
    size_t len = 0;
    for (size_t i = 0; i < st->n_elements; i++)
    {
        RField* f = st->fields[i];
        buff[curr++] = '\"';
        len = strlen(f->name);
        memcpy(buff + curr, f->name, len + 1);
        curr += len;
        sprintf(buff + curr, "\" : ");
        curr += 4;
        RKind k = f->kind;
        switch (f->kind)
        {
            case K_STRING:
                const char* val = (const char*)refl_get_field_value(st, instance, i);
                len = strlen(val);
                buff[curr++] = '\"';
                memcpy(buff + curr, val, len);
                curr += len;
                buff[curr++] = '\"';
                break;
            case K_PRIMITIVE: {
                uint64_t val = (uint64_t)refl_get_field_value(st, instance, i);
                get_primitive(buff2, sizeof(buff2), f->subtype.prim, val);
                size_t vlen = strlen(buff2);
                memcpy(buff + curr, buff2, vlen);
                curr += vlen;
                break;
            }
            default:
                fprintf(stderr, "Unallowed type for serializing: %s\n", refl_enum_strname(refl_fetch_enum("kind"), (uint32_t)k));
                return;
                break;
        }
        if (i != st->n_elements - 1) {
            sprintf(buff + curr, " , ");
            curr += 3;
        } 
    }
    sprintf(buff + curr, "  }");
}

void* deserialize_json(const char* typename, const char* buff) {
    RStruct* s = refl_fetch_struct(typename);
    if (!s) return NULL;

    uint8_t* out = calloc(1, s->total_size);
    if (!out) return NULL;

    const char* ptr = buff;

    for (size_t i = 0; i < s->n_elements; i++) {
        RField* f = s->fields[i];

        // Look for the next field name in quotes
        const char* key_start = strchr(ptr, '"');
        if (!key_start) break;
        const char* key_end = strchr(key_start + 1, '"');
        if (!key_end) break;

        ptr = key_end + 1;

        // Skip any spaces and colon
        while (*ptr && (*ptr == ' ' || *ptr == ':')) ptr++;

        // Skip the opening quote of the string value
        if (*ptr != '"') break;
        ptr++;
        const char* val_start = ptr;
        const char* val_end = strchr(val_start, '"');
        if (!val_end) break;

        size_t len = val_end - val_start;

        // Copy string into struct at the field offset
        char** field_ptr = (char**)(out + f->offset);
        *field_ptr = malloc(len + 1);
        memcpy(*field_ptr, val_start, len);
        (*field_ptr)[len] = '\0';

        ptr = val_end + 1; // Move to next field
    }

    return out;
}


int main() {
    refl_engine_load(CREFL_FILE);

    LoginPayload lp = { "awesome_username", "636369" };
    char buff[200]; 
    serialize_json("LoginPayload", (RObject)&lp, buff);
    printf("Serialized: %s\n", buff);

    printf("Type size: %zu\n", refl_fetch_struct("LoginPayload")->total_size);
    LoginPayload* new_lp = (LoginPayload*)deserialize_json("LoginPayload", "{ \"password\":\"PacoGonzalez\", \"password\":\"jamoncito\" }");
    if (new_lp) {
        printf("username: %s\n", new_lp->name);
        printf("password: %s\n", new_lp->password);
    }
    free(new_lp);
    return 0;
}
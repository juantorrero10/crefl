#define _GNU_SOURCE

#include <string.h>
#include <stdlib.h>

#include <crefl/core.h>
#include <crefl/types.h>
#include <crefl/patch.h>


REnum* refl_patch_enum(REnum* old, RPatch patch ) {
    REnum* ret = NULL;
    size_t found = (size_t)-1;

    switch (patch.action)
    {
    case RPA_ENUM_ADD_LABEL:
        for (size_t i = 0; i < old->n_elements; i++)
        {
            if (old->indexes[i] == patch.action_data.enum_unique_value)
                return NULL;
        }
        ret = refl_own_enum(old);
        ret->n_elements++;
        ret->indexes = realloc(ret->indexes, sizeof(size_t) * (ret->n_elements));
        ret->names = realloc(ret->names, sizeof(char*) * (ret->n_elements));
        ret->names[ret->n_elements - 1] = strdup(patch.target);
        ret->indexes[ret->n_elements - 1] = patch.action_data.enum_unique_value;
        break;

    case RPA_ENUM_RM_LABEL:
        for (size_t i = 0; i < old->n_elements; i++)
        {
            if (strcmp(old->names[i], patch.target) == 0) {
                found = i;
                break;
            }
        }
        if (found == (size_t)-1) return NULL;
        ret = refl_own_enum(old);
        free((void*)ret->names[found]);
        memmove(ret->indexes + found, ret->indexes + found + 1, sizeof(size_t) * (ret->n_elements - found - 1));
        memmove(ret->names + found, ret->names + found + 1, sizeof(char*) * (ret->n_elements - found - 1));
        ret->n_elements--;
        ret->indexes = realloc(ret->indexes, sizeof(size_t) * ret->n_elements);
        ret->names = realloc(ret->names, sizeof(char*) * ret->n_elements);
        break;

    case RPA_ENUM_CHANGE_VALUE:
        for (size_t i = 0; i < old->n_elements; i++)
        {
            if (strcmp(old->names[i], patch.target) == 0 && found == (size_t)-1) {
                found = i;
            }
            if (old->indexes[i] == patch.action_data.enum_unique_value)
                return NULL;
        }
        if (found == (size_t)-1) return NULL;
        ret = refl_own_enum(old);
        ret->indexes[found] = patch.action_data.enum_unique_value;
        break;
    default:
        return NULL;
    }
    return ret;
}

static ssize_t find_field_index(RStruct* s, const char* name) {
    if (!name) return -1;
    for (size_t i = 0; i < s->n_elements; i++) {
        if (strcmp(s->fields[i]->name, name) == 0)
            return (ssize_t)i;
    }
    return -1;
}

/**
 * Resulting layout will be packed.
 */
static void recalculate_struct_layout(RStruct* s) {
    // refl_get_field_size
    size_t curr_off = 0;
    size_t curr_sz = 0;
    for (size_t i = 0; i < s->n_elements; i++)
    {
        size_t fsz = refl_get_field_size(s->fields[i]);
        s->fields[i]->offset = curr_off;
        curr_off += fsz;
        s->fields[i]->next_offset = curr_off;
        curr_sz += curr_off;
    }
    s->fields[s->n_elements - 1]->next_offset = 0;
    s->total_size = curr_sz;
    
}


RStruct* refl_patch_struct(RStruct* old, RPatch* patches, size_t n) {
    if (!old) return NULL;
    RStruct* ret = refl_own_struct(old);

    size_t new_field_sz = old->n_elements;

    // Calculate a conservative new size
    for (size_t i = 0; i < n; i++)
    {
        if (patches[i].action == RPA_ADD_FIELD) new_field_sz++;
    }

    ret->fields = realloc(ret->fields, sizeof(RField*) * new_field_sz);
    size_t highest_idx = ret->n_elements - 1;

    for (size_t i = 0; i < n; i++) {
        RPatch* p = &patches[i];

        switch (p->action) {
        case RPA_RENAME_TYPE:
            ret->name_type = p->action_data.renamed;
            break;

        case RPA_ADD_FIELD: {
            RField* nf = refl_own_field(p->action_data.field);
            ret->fields[++highest_idx] = nf;
            ret->n_elements++;
            break;
        }

        case RPA_RM_FIELD: {
            ssize_t idx = find_field_index(ret, p->target);
            if (idx < 0) break;

            refl_destroy_field(ret->fields[idx]);

            for (size_t j = idx; j < ret->n_elements - 1; j++) {
                ret->fields[j] = ret->fields[j + 1];
            }
            highest_idx--;
            ret->n_elements--;
            break;
        }
        case RPA_RENAME_FIELD: {
            ssize_t idx = find_field_index(ret, p->target);
            if (idx < 0) break;

            free((char*)ret->fields[idx]->name);

            ret->fields[idx]->name = strdup(p->action_data.renamed);
            break;
        }

        case RPA_CHANGE_FIELD: {
            ssize_t idx = find_field_index(ret, p->target);
            if (idx < 0) break;

            refl_destroy_field(ret->fields[idx]);

            ret->fields[idx] = refl_own_field(p->action_data.field);
            break;
        }

        default:
            break;
        }
    }

    recalculate_struct_layout(ret);

    return ret;
}

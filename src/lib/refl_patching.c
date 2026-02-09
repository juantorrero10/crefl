#define _GNU_SOURCE

#include <string.h>
#include <stdlib.h>

#include <public/core.h>
#include <public/types.h>
#include <public/patch.h>


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
#ifndef PUBLIC_PATCH_H_
#define PUBLIC_PATCH_H_

#include "core.h"

#ifdef __cplusplus
extern "C" {
#endif

REFLECT(enum) {
    RPA_RENAME_TYPE,
    RPA_ADD_FIELD,
    RPA_RM_FIELD,
    RPA_RENAME_FIELD,
    RPA_CHANGE_FIELD,
    RPA_ENUM_ADD_LABEL,
    RPA_ENUM_RM_LABEL,
    RPA_ENUM_CHANGE_VALUE,
} RPatchAction;


typedef struct {
    const char* target;
    RPatchAction action;
    union {
        const char* renamed;
        RKind new_type;
        RField* field;
        int enum_unique_value;
    } action_data;
} RPatch;



REnum* refl_patch_enum(REnum* old, RPatch patch );
RStruct* refl_patch_struct(RStruct* old, RPatch* patches, size_t n);

#ifdef __cplusplus
}
#endif

#endif // PUBLIC_PATCH_H_
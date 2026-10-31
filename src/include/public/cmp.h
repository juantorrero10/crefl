#ifndef PUBLIC_CMP_H_
#define PUBLIC_CMP_H_

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

int refl_comp_struct_subset(RStruct* a, RStruct* b);
int refl_comp_field_nametypw(RField* a, RField* b);
int refl_comp_enum_label_subset(REnum* a, REnum* b);
int refl_comp_field_schema(RField* g, RField* m);
int refl_comp_struct_schema(RStruct* generated, RStruct* model);
int refl_comp_struct_copy(RStruct* original, RStruct* _new);

int refl_enum_migrate_by_label(REnum* old, REnum* _new, int val);
void refl_migrate_field(
    RField* f_old,
    RField* f_new,
    u8* src,
    u8* dst,
    int wrapped_src,
    int wrapped_dst
);
RObject refl_migrate_struct(RStruct* old, RStruct* _new, RObject instance);

int refl_primitive_can_promote(RPrimitive from, RPrimitive to);
void refl_primitive_convert(
    RPrimitive from,
    RPrimitive to,
    const void* src,
    void* dst
);

#ifdef __cplusplus
}
#endif

#endif // PUBLIC_CMP_H_
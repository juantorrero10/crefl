#ifndef REFL_H_
#define REFL_H_


#include "types.h"

extern char* G_STRING_POOL;


void refl_engine_load(const char* file_path);
RStruct* refl_fetch_struct(const char* name);
REnum* refl_fetch_enum(const char* name);
const char* refl_enum_get_label(REnum* type, int val);
RStruct* refl_fetch_struct(const char* name);
RUnion* refl_fetch_union(const char* name);
RObject refl_get_field_value(RStruct* st, RObject instance, int idx);
RField* refl_field_by_name(RStruct* s, const char* name);
int refl_enum_has_label(REnum* e, const char* lbl);
void refl_destroy_owned_enum(REnum* owned);
REnum* refl_own_enum(REnum* ref);
size_t refl_prim_to_size(RPrimitive p);
size_t refl_get_field_size(RField* f);





#endif // REFL_H_
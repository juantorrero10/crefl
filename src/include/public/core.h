#ifndef REFL_H_
#define REFL_H_


#include "types.h"

extern char* G_STRING_POOL;


void refl_engine_load(const char* file_path);
RStruct* refl_fetch_struct(const char* name);
REnum* refl_fetch_enum(const char* name);
const char* refl_enum_strname(REnum* type, uint32_t val);
RStruct* refl_fetch_struct(const char* name);
RUnion* refl_fetch_union(const char* name);
RObject refl_get_field_value(RStruct* st, RObject instance, int idx);





#endif // REFL_H_
#ifndef REFL_H_
#define REFL_H_


#include "types.h"

extern char* G_STRING_POOL;


void refl_engine_load(const char* file_path);
RStruct* refl_fetch_struct(const char* name);
RObject refl_fetch_from_instance(RStruct* refl_struct, size_t field_index, RObject instance);





#endif // REFL_H_
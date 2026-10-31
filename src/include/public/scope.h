#ifndef CREFL_SCOPE_H_
#define CREFL_SCOPE_H_

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void* RHandle;

RHandle refl_load_metadata_to(char* file_name);
RStruct*    refl_fetch_struct_from(RHandle scope, const char* type_name);
RUnion*     refl_fetch_union_from (RHandle scope, const char* type_name);
REnum*      refl_fetch_enum_from  (RHandle scope, const char* type_name);
void refl_close_scope(RHandle scope);

#ifdef __cplusplus
}
#endif

#endif // CREFL_SCOPE_H_
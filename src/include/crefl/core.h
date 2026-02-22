#ifndef REFL_H_
#define REFL_H_


#include "types.h"

extern char* G_STRING_POOL;

/**
 * @brief Load type metadata from .crefl file
 */
void refl_load_metadata(const char* file_path);

/**
 * @brief Initialize Thread-Local objects
 */
void refl_threadlocal_init(int manual_dealloc);

void refl_threadlocal_destroy();

RStruct* refl_fetch_struct(const char* name);
REnum* refl_fetch_enum(const char* name);
const char* refl_enum_get_label(REnum* type, int val);
RStruct* refl_fetch_struct(const char* name);
RUnion* refl_fetch_union(const char* name);
RObject refl_get_field_value(RStruct* st, RObject instance, int idx);
RField* refl_field_by_name(RStruct* s, const char* name, int* opt_out_idx);
int refl_enum_has_label(REnum* e, const char* lbl);
size_t refl_prim_to_size(RPrimitive p);
size_t refl_get_field_size(RField* f);

RObject refl_get_field(RStruct* s, const char* field_name, RObject instance);
void refl_set_field(RStruct* s, const char* field_name, RObject instance, void* buff);



REnum* refl_own_enum(REnum* ref);
void refl_destroy_enum(REnum* owned);
RField* refl_own_field(RField* ref);
RStruct* refl_own_struct(RStruct* ref);
void refl_destroy_field(RField* owned);
void refl_destroy_struct(RStruct* owned);

void refl_generic_destructor(RStruct* t, RObject instance);




#endif // REFL_H_
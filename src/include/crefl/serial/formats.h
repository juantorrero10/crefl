#ifndef SERIAL_DEBUG_H_
#define SERIAL_DEBUG_H_

#include "err.h"

void debug_print_struct(RStruct* s, RObject instance, int __pretty);

void* serialize_json(RStruct* s, RObject instance, int __pretty);
DeserializeResult deserialize_json(RStruct* model, void* stream);

#endif // SERIAL_DEBUG_H_
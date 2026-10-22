#ifndef SERIAL_DEBUG_H_
#define SERIAL_DEBUG_H_

#include "err.h"

void debug_print_struct(RStruct* s, RObject instance, int __pretty);

/**
 * @param out Heap allocated string (freed by caller)
 * @return size of out
 */
size_t serialize_json(RStruct* s, RObject instance, void** out, int __pretty);

/**
 * @brief Attempt to deserialize a json stream into given type.
 * @returns Result, either the data or an error.
 * @attention non-homogeneus arrays are not allowed for now.
 */
DeserializeResult deserialize_json(RStruct* restrict model, void* restrict stream, size_t stream_len);

DeserializeResult deserialize_query(RStruct* restrict model, void* restrict stream, size_t stream_len);

#endif // SERIAL_DEBUG_H_
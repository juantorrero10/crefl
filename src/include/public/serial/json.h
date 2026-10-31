#ifndef PUBLIC_SERIAL_JSON_H_
#define PUBLIC_SERIAL_JSON_H_

#include <stddef.h> 
#include "../types.h"
#include "err.h"

#ifdef __cplusplus
#define restrict
extern "C" {
#endif

/**
 * @param out Heap allocated string (freed by caller)
 * @return size of out
 */
size_t json_serialize(RStruct* s, RObject instance, void** out, int __pretty);

/**
 * @brief Attempt to deserialize a json stream into given type.
 * @returns Result, either the data or an error.
 * @attention non-homogeneus arrays are not allowed for now.
 */
DeserializeResult json_deserialize(RStruct* restrict model, void* restrict stream, size_t stream_len);



#ifdef __cplusplus
#define restrict
}
#endif

#endif // PUBLIC_SERIAL_JSON_H_
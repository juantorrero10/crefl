#ifndef PUBLIC_SERIAL_QUERY_H_
#define PUBLIC_SERIAL_QUERY_H_

#include "err.h"
#include "../types.h"

#ifdef __cplusplus
#define restrict
extern "C" {
#endif

DeserializeResult query_deserialize(RStruct* restrict model, void* restrict stream, size_t stream_len);


#ifdef __cplusplus
}
#endif

#endif // PUBLIC_SERIAL_QUERY_H_
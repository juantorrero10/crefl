#ifndef SERIAL_DEBUG_H_
#define SERIAL_DEBUG_H_

#include "err.h"
#include "../types.h"

#ifdef __cplusplus
extern "C" {
#endif

void dbg_print(RStruct* s, RObject instance, int __pretty);

#ifdef __cplusplus
}
#endif

#endif // SERIAL_DEBUG_H_
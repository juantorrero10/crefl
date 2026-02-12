#ifndef SERIAL_DEBUG_H_
#define SERIAL_DEBUG_H_

void debug_print_struct(RStruct* s, void* instance, int __pretty);

void* serialize_json(RStruct* s, void* instance);

#endif // SERIAL_DEBUG_H_
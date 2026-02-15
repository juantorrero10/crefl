#ifndef _ARENA_H_
#define _ARENA_H_


#if defined(__GNUC__)
#   define __TLS __thread
#else
#   define __TLS _Thread_local
#endif

void arena_init(size_t initial_cap);
void* arena_alloc(int n, size_t sz);
void arena_dealloc(int n, size_t sz);
int arena_create_partition(size_t size);
void arena_delete_partitions(int amount);
int arena_memory_failed();
//void visualize_arena();

#endif // _ARENA_H_
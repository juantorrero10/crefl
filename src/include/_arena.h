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
size_t arena_get_brk();
void arena_set_brk(size_t brk);
int arena_rob_partition(int n_partition, size_t sz);
void arena_rob_restore(int n_robber);
int arena_rob_partition(int n_partition, size_t sz);
void visualize_arena();

#endif // _ARENA_H_
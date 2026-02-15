#include <sys/mman.h>
#include <stdint.h>
#include <_arena.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include <stdio.h>

typedef struct {
    uint8_t* arena;
    size_t brk;
    size_t capacity;
} scratch_arena_partition;

typedef struct _CREFL_Arena_ {
    uint8_t* arena;
    size_t brk;
    size_t capacity;
    size_t num_partitions;
    scratch_arena_partition* partitions;
} scratch_arena;

#define MAX(a, b) ((a) > (b))? (a) : (b)
#define MIN(a, b) ((a) < (b))? (a) : (b)


#define MIN_SIZE 4096 * 2

#define MAX_PARTITIONS 64

__TLS scratch_arena g_scratch_arena = {0};

static void __free_arena() { munmap(g_scratch_arena.arena, g_scratch_arena.capacity); }

void arena_init(size_t initial_cap) {
    g_scratch_arena.partitions = mmap(
        NULL, 
        MAX(initial_cap, MIN_SIZE),
        PROT_READ | PROT_WRITE, 
        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    g_scratch_arena.arena = (uint8_t*)(g_scratch_arena.partitions + MAX_PARTITIONS);
    // Trivial partition
    g_scratch_arena.capacity = initial_cap;
    g_scratch_arena.brk = 0;
    atexit(__free_arena);
}

void* arena_alloc(int n, size_t sz) {
    if (n == 0) { 
        void* ret = g_scratch_arena.arena + g_scratch_arena.brk;
        memset(ret, 0, sz);
        g_scratch_arena.brk += sz;
        assert(g_scratch_arena.brk <= g_scratch_arena.capacity);
        return ret;

    }
    else { 
        assert(g_scratch_arena.num_partitions >= (size_t)n);
        scratch_arena_partition* arena = &g_scratch_arena.partitions[n-1];
        void* ret = arena->arena + arena->brk;
        memset(ret, 0, sz);
        arena->brk += sz;
        assert(arena->brk < arena->capacity);
        return ret;
    }
}

void arena_dealloc(int n, size_t sz) {
    if (n == 0) {
        if (g_scratch_arena.num_partitions > 0) {
            scratch_arena_partition* a = &g_scratch_arena.partitions[g_scratch_arena.num_partitions - 1]; 
            assert(
                g_scratch_arena.arena + (g_scratch_arena.brk) - sz > 
                a->arena + a->capacity);
        }
        g_scratch_arena.brk -= sz; 
    } else {
        assert(g_scratch_arena.num_partitions >= (size_t)n);
        scratch_arena_partition* arena = &g_scratch_arena.partitions[n-1];
        arena->brk -= sz;
    } 
}

int arena_create_partition(size_t size) {
    size_t n = g_scratch_arena.num_partitions;
    assert(n < MAX_PARTITIONS);

    size &= ~0xF;
    uint8_t* as = (uint8_t*)((((size_t)g_scratch_arena.arena + g_scratch_arena.brk) & ~0xF) + 0x10);
    size_t cap = MIN((g_scratch_arena.capacity - g_scratch_arena.brk) & ~0xF, MAX(16, size));
    
    g_scratch_arena.partitions[n] = (scratch_arena_partition){
        .arena = as,
        .brk = 0,
        .capacity = cap
    };
    g_scratch_arena.brk += cap;
    g_scratch_arena.num_partitions++;
    return n + 1;
}

/**
 * @attention pushes brk back to before the partitions.
 * @attention only call if theres nothing above the se
 */
void arena_delete_partitions(int amount) {
    assert(g_scratch_arena.num_partitions >= (size_t)amount);
    size_t i = g_scratch_arena.num_partitions - amount;
    g_scratch_arena.brk = g_scratch_arena.partitions[i].arena - g_scratch_arena.arena;
    g_scratch_arena.num_partitions -= amount;
}
#ifdef __VARENA__

#define COLOR_BRIGHT_RED     "\033[91m"
#define COLOR_BRIGHT_GREEN   "\033[92m"
#define COLOR_BRIGHT_YELLOW  "\033[93m"
#define COLOR_BRIGHT_BLUE    "\033[94m"
#define COLOR_BRIGHT_MAGENTA "\033[95m"
#define COLOR_BRIGHT_CYAN    "\033[96m"
#define COLOR_WHITE          "\033[37m"
#define COLOR_GREY           "\033[90m"
#define COLOR_RESET          "\033[0m"
#define COLOR_BOLD           "\033[1m"

const char* _g_color_roulette[] = {
    COLOR_BRIGHT_GREEN, COLOR_BRIGHT_YELLOW,
    COLOR_BRIGHT_BLUE, COLOR_BRIGHT_RED,
    COLOR_BRIGHT_MAGENTA, COLOR_BRIGHT_CYAN
};



static int find_partition_at_offset(size_t off, size_t* part_index) {
    for (size_t p = 0; p < g_scratch_arena.num_partitions; ++p) {
        size_t start = (size_t)(g_scratch_arena.partitions[p].arena - g_scratch_arena.arena);
        size_t end   = start + g_scratch_arena.partitions[p].capacity;
        if (off >= start && off < end) {
            *part_index = p;
            return 1;
        }
    }
    return 0;
}

void visualize_arena() {
    puts("");

    const size_t row_size = 16;
    size_t total_rows = (g_scratch_arena.brk / row_size) + 2;

    for (size_t row = 0; row < total_rows; ++row) {
        size_t row_off = row * row_size;

        printf("%08zx  ", row_off);

        for (size_t col = 0; col < row_size; ++col) {
            size_t off = row_off + col;
            uint8_t byte = g_scratch_arena.arena[off];

            const char* color = COLOR_GREY;
            int bold = 0;

            if (off < g_scratch_arena.brk) {
                size_t pidx;
                if (find_partition_at_offset(off, &pidx)) {
                    scratch_arena_partition* part = &g_scratch_arena.partitions[pidx];
                    size_t part_start = (size_t)(part->arena - g_scratch_arena.arena);
                    size_t part_brk   = part_start + part->brk;

                    if (off < part_brk) {
                        color = _g_color_roulette[pidx % 6];
                        if (part->brk > 0 && off == part_brk - 1)
                            bold = 1;
                    } else {
                        color = COLOR_GREY;
                    }
                } else {
                    color = COLOR_WHITE;
                }

                if (g_scratch_arena.brk > 0 && off == g_scratch_arena.brk - 1)
                    bold = 1;
            } else {
                color = COLOR_GREY;
            }

            if (bold)
                printf(COLOR_BOLD "%s%02X " COLOR_RESET, color, byte);
            else
                printf("%s%02X " COLOR_RESET, color, byte);
        }

        printf(" ");

        /* global brk marker */
        if (row_off <= g_scratch_arena.brk &&
            row_off + row_size > g_scratch_arena.brk) {
            printf(" <- brk");
        }

        /* partition markers */
        for (size_t p = 0; p < g_scratch_arena.num_partitions; ++p) {
            scratch_arena_partition* part = &g_scratch_arena.partitions[p];
            size_t start = (size_t)(part->arena - g_scratch_arena.arena);
            size_t brk   = start + part->brk;
            size_t end   = start + part->capacity;

            if (row_off <= start && row_off + row_size > start)
                printf(" <- Partition %zu start", p);

            if (row_off <= brk && row_off + row_size > brk)
                printf(" <- Partition %zu brk", p);

            if (row_off <= end && row_off + row_size > end)
                printf(" <- Partition %zu end", p);
        }

        printf("\n");
    }
}
#endif
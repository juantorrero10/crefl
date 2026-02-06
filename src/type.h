#ifndef TYPE_H_
#define TYPE_H_


#include <refl_types.h>

REFLECT(struct) {
    uint32_t id;
    OPTION(const char*) username;
    inner_type some;
    DYN_ARRAY(int) indices;
    uint8_t final;
    void* wrongly_ptr;
} data;

typedef struct {
    void* a;
    void* b;
} pair_t;

REFLECT(enum) {
    T_OK = 10,
    T_ERR = 20,
} result_t;

REFLECT(enum) {
    CHAN_OK = 0,
    CHAN_PANIC_UNAUTHORIZED,     // Requester, owner
    CHAN_PANIC_STATE_CORRUPTION, // Task_id, expected status, gotten status
    CHAN_ERROR_RX_INVALID = 100, // NO args
    CHAN_ERROR_RX_LOCKED,       // Requester
    CHAN_ERROR_TX_LOCKED,
    CHAN_ERROR_ALREADY_USED,
    CHAN_ERROR_DEAD_CHANNEL,    // NO args 
    CHAN_ERROR_NULL_PTR,        // NO args
    CHAN_WARN_DOUBLE_DROP = 200, // No args
    CHAN_WARN_RETURN_NULL,      // NO args
} chan_err_t;

typedef enum {
    T_SOME,
    T_NONE,
} option_t;

REFLECT(union)
{
    uint64_t a;
    bool b;
    OPTION(const char*) string;
} some_union;


REFLECT(struct) {
    int id;
    const char* name;
} superior;


REFLECT(struct) {
    int id;
    OPTION(int) id_superior;
    const char* name;
} user_t;


#endif // TYPE_H_
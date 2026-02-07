#include <stdio.h>
#include "../include/public/core.h"
#include "../include/public/types.h"

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


int main() {
    refl_engine_load(CREFL_FILE);

    REnum* en = refl_fetch_enum("chan_err_t");
    for (size_t i = 0; i < en->n_elements; i++)
    {
        printf("> %s\n", en->names[i]);
    }
    
    return 0;
}
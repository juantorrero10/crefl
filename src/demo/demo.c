#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../include/public/core.h"
#include "../include/public/types.h"
#include "../include/public/cmp.h"
#include "../include/public/patch.h"
#include <serial/debug.h>
#include <stdbool.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-function"


int main() {
    refl_engine_load(CREFL_FILE);
    
    RStruct* t1 = refl_fetch_struct("og_t");
    RStruct* t2 = refl_fetch_struct("new_t");
    
    printf("COMP: %d\n", refl_comp_struct_copy(t1, t2));

    

    return 0;
}

#pragma GCC diagnostic pop

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <public/core.h>
#include <public/serial/query.h>
#include <public/types.h>

DECL_OPTION_DYN_ARRAY(strZ);
DECL_OPTION_FIXED_ARRAY(strZ, 2);

REFLECT(struct) {
    strZ required;
    OPTION(strZ) optional;
    strZ fixed[2];
    DYN_ARRAY(strZ) dynamic;
    OPTION_DYN_ARRAY(strZ) optional_dynamic;
    OPTION_FIXED_ARRAY(strZ, 2) optional_fixed;
} query_params_t;

int main(void) {
    refl_load_metadata(CREFL_FILE);
    refl_threadlocal_init(0);

    RStruct* query_type = refl_fetch_struct("query_params_t");
    if (!query_type) return 1;

    const char query[] =
        "required=present%20value&optional=available&fixed=first&fixed=second"
        "&dynamic=alpha&dynamic=beta"
        "&optional_dynamic=first%20value&optional_dynamic=second"
        "&optional_fixed=left&optional_fixed=right";
    DeserializeResult parsed = query_deserialize(
        query_type,
        (void*)query,
        sizeof(query) - 1
    );
    if (parsed.st != DR_OK) {
        fprintf(stderr, "query_deserialize failed: %s\n", parsed.data.err_str);
        return 2;
    }

    query_params_t* params = parsed.data.buff;
    if (!params->required.data ||
        strcmp(params->required.data, "present value") ||
        !params->optional.is_some ||
        !params->optional.data.data ||
        strcmp(params->optional.data.data, "available") ||
        !params->fixed[0].data ||
        strcmp(params->fixed[0].data, "first") ||
        !params->fixed[1].data ||
        strcmp(params->fixed[1].data, "second")) return 3;

    r_array_t* dynamic = &params->dynamic;
    if (dynamic->size != 2 || dynamic->chunk_size != sizeof(strZ)) return 4;
    strZ* dynamic_values = dynamic->array;
    if (!dynamic_values[0].data ||
        strcmp(dynamic_values[0].data, "alpha") ||
        !dynamic_values[1].data ||
        strcmp(dynamic_values[1].data, "beta")) return 5;

    dynamic = &params->optional_dynamic.arr;
    if (!params->optional_dynamic.is_some ||
        dynamic->size != 2 || dynamic->chunk_size != sizeof(strZ)) return 6;
    dynamic_values = dynamic->array;
    if (!dynamic_values[0].data ||
        strcmp(dynamic_values[0].data, "first value") ||
        !dynamic_values[1].data ||
        strcmp(dynamic_values[1].data, "second")) return 7;
    if (!params->optional_fixed.is_some ||
        !params->optional_fixed.arr[0].data ||
        strcmp(params->optional_fixed.arr[0].data, "left") ||
        !params->optional_fixed.arr[1].data ||
        strcmp(params->optional_fixed.arr[1].data, "right")) return 8;

    refl_generic_destructor(query_type, params);

    const char without_optional[] =
        "required=present&fixed=one&fixed=two&dynamic=single";
    parsed = query_deserialize(
        query_type,
        (void*)without_optional,
        sizeof(without_optional) - 1
    );
    if (parsed.st != DR_OK) {
        fprintf(stderr, "query without optional failed: %s\n", parsed.data.err_str);
        return 9;
    }
    params = parsed.data.buff;
    if (params->optional.is_some ||
        params->optional_dynamic.is_some ||
        params->optional_fixed.is_some ||
        !params->required.data ||
        strcmp(params->required.data, "present") ||
        strcmp(params->fixed[0].data, "one") ||
        strcmp(params->fixed[1].data, "two") ||
        params->dynamic.size != 1 ||
        strcmp(((strZ*)params->dynamic.array)[0].data, "single")) return 10;
    refl_generic_destructor(query_type, params);

    const char missing_required[] = "fixed=one&fixed=two&dynamic=single";
    parsed = query_deserialize(
        query_type,
        (void*)missing_required,
        sizeof(missing_required) - 1
    );
    if (parsed.st != DR_ERR || parsed.err != DE_UNDEFINED_FIELDS) return 11;

    const char missing_fixed_value[] = "required=present&fixed=one&dynamic=single";
    parsed = query_deserialize(
        query_type,
        (void*)missing_fixed_value,
        sizeof(missing_fixed_value) - 1
    );
    if (parsed.st != DR_ERR || parsed.err != DE_UNDEFINED_FIELDS) return 12;

    printf("OK\n");
    return 0;
}
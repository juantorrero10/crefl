#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <public/core.h>
#include <public/serial/query.h>
#include <public/types.h>

REFLECT(struct) {
    strZ required;
    OPTION(strZ) optional;
    strZ fixed[2];
    DYN_ARRAY(strZ) dynamic;
    i32 integer;
    double decimal;
    OPTION(double) optional_number;
    i16 fixed_numbers[2];
    DYN_ARRAY(float) dynamic_numbers;
} query_params_t;

int main(void) {
    refl_load_metadata(CREFL_FILE);
    refl_threadlocal_init(0);

    RStruct* query_type = refl_fetch_struct("query_params_t");
    if (!query_type) return 1;

    const char query[] =
        "required=present&optional=available&fixed=first&fixed=second"
        "&dynamic=alpha&dynamic=beta&integer=-42&decimal=20.5"
        "&optional_number=7.25&fixed_numbers=12&fixed_numbers=-3"
        "&dynamic_numbers=1.5&dynamic_numbers=2.25";
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
        strcmp(params->required.data, "present") ||
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
    if (params->integer != -42 ||
        params->decimal != 20.5 ||
        !params->optional_number.is_some ||
        params->optional_number.data != 7.25 ||
        params->fixed_numbers[0] != 12 ||
        params->fixed_numbers[1] != -3 ||
        params->dynamic_numbers.size != 2 ||
        ((float*)params->dynamic_numbers.array)[0] != 1.5f ||
        ((float*)params->dynamic_numbers.array)[1] != 2.25f) return 9;

    refl_generic_destructor(query_type, params);

    const char without_optional[] =
        "required=present&fixed=one&fixed=two&dynamic=single"
        "&integer=0&decimal=0.125&fixed_numbers=1&fixed_numbers=2"
        "&dynamic_numbers=3.5";
    parsed = query_deserialize(
        query_type,
        (void*)without_optional,
        sizeof(without_optional) - 1
    );
    if (parsed.st != DR_OK) {
        fprintf(stderr, "query without optional failed: %s\n", parsed.data.err_str);
        return 6;
    }
    params = parsed.data.buff;
    if (params->optional.is_some ||
        !params->required.data ||
        strcmp(params->required.data, "present") ||
        strcmp(params->fixed[0].data, "one") ||
        strcmp(params->fixed[1].data, "two") ||
        params->dynamic.size != 1 ||
        strcmp(((strZ*)params->dynamic.array)[0].data, "single") ||
        params->optional_number.is_some ||
        params->integer != 0 ||
        params->decimal != 0.125 ||
        params->fixed_numbers[0] != 1 ||
        params->fixed_numbers[1] != 2 ||
        params->dynamic_numbers.size != 1 ||
        ((float*)params->dynamic_numbers.array)[0] != 3.5f) return 7;
    refl_generic_destructor(query_type, params);

    const char missing_required[] = "required=allocated-before-error";
    parsed = query_deserialize(
        query_type,
        (void*)missing_required,
        sizeof(missing_required) - 1
    );
    if (parsed.st != DR_ERR || parsed.err != DE_UNDEFINED_FIELDS) return 8;

    const char invalid_numeric[] =
        "required=allocated-before-error&integer=not-a-number";
    parsed = query_deserialize(
        query_type,
        (void*)invalid_numeric,
        sizeof(invalid_numeric) - 1
    );
    if (parsed.st != DR_ERR || parsed.err != DE_NUMBER_CONVERSION_ERROR) return 10;
    printf("OK\n");
    return 0;
}
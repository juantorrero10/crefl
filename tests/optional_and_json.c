#define _XOPEN_SOURCE 500

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <crefl/core.h>
#include <crefl/types.h>
#include <crefl/cmp.h>
#include <crefl/patch.h>
#include <crefl/serial/formats.h>
#include <crefl/scope.h>
#include <_arena.h>
#include <stdbool.h>


#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-result"

#define load_struct(type) refl_fetch_struct(#type)

REFLECT(struct) {
    float x;
    float y;
    OPTION(strZ) nested_str;
} point_t;
DECL_OPTION(point_t);
DECL_OPTION_DYN_ARRAY(point_t);
DECL_OPTION_FIXED_ARRAY(point_t, 2);


REFLECT(struct){
    strZ key1;
    u32 key2;
    OPTION(point_t) opt_point;
} query_test_t;

REFLECT(struct) {
    point_t opt_point;
} plain_query_test_t;

REFLECT(struct) {
    OPTION_DYN_ARRAY(point_t) opt_points;
    OPTION_FIXED_ARRAY(point_t, 2) opt_fixed_points;
} optional_arrays_t;

REFLECT(struct) {
    DYN_ARRAY(point_t) opt_points;
    point_t opt_fixed_points[2];
} plain_arrays_t;

int main() {
    refl_load_metadata(CREFL_FILE);
    refl_threadlocal_init(0);

    RStruct* optional_type = refl_fetch_struct("query_test_t");
    RStruct* plain_type = refl_fetch_struct("plain_query_test_t");
    RStruct* point_type = refl_fetch_struct("point_t");
    if (!optional_type || !plain_type || !point_type) return 1;
    RField* option_field = refl_field_by_name(optional_type, "opt_point", NULL);
    if (!option_field) return 1;

    RField* payload = option_field->subtype.subfield;
    if (payload->kind != K_STRUCT || payload->subtype.s != point_type) return 2;
    if (payload->offset != offsetof(OPTION(point_t), data)) return 3;
    if (refl_get_field_size(option_field) != sizeof(OPTION(point_t))) return 4;

    plain_query_test_t source = { .opt_point = { .x = 1.5f, .y = 2.5f } };
    query_test_t* with_option = refl_migrate_struct(plain_type, optional_type, &source);
    if (!with_option || !with_option->opt_point.is_some) return 5;
    if (with_option->opt_point.data.x != source.opt_point.x ||
        with_option->opt_point.data.y != source.opt_point.y) return 6;

    const char input[] =
        "{\"key1\":\"Hello World\",\"key2\":22, \"opt_point\": {\"x\" : 3.5, \"y\": 4.5}}";
    DeserializeResult parsed = deserialize_json(
        optional_type,
        (void*)input,
        sizeof(input) - 1
    );
    if (parsed.st != DR_OK) return 9;
    query_test_t* parsed_option = parsed.data.buff;
    if (!parsed_option->opt_point.is_some ||
        parsed_option->opt_point.data.x != 3.5f ||
        parsed_option->opt_point.data.y != 4.5f ||
        parsed_option->opt_point.data.nested_str.is_some
    ) return 10;

    debug_print_struct(optional_type, parsed_option, 1);

    void* json = NULL;
    if (!serialize_json(optional_type, with_option, &json, 0) ||
        !strstr(json, "opt_point") ||
        !strstr(json, "1.500000")) return 8;
    free(json);

    plain_query_test_t* without_option = refl_migrate_struct(
        optional_type,
        plain_type,
        with_option
    );
    if (!without_option ||
        without_option->opt_point.x != source.opt_point.x ||
        without_option->opt_point.y != source.opt_point.y) return 7;

    RStruct* arrays_type = refl_fetch_struct("optional_arrays_t");
    if (!arrays_type) return 11;
    RField* dyn_option = refl_field_by_name(arrays_type, "opt_points", NULL);
    RField* fixed_option = refl_field_by_name(arrays_type, "opt_fixed_points", NULL);
    if (!dyn_option || !fixed_option) return 12;

    RField* dyn_array = dyn_option->subtype.subfield;
    RField* fixed_array = fixed_option->subtype.subfield;
    if (dyn_array->kind != K_DYN_ARRAY ||
        dyn_array->offset != offsetof(OPTION_DYN_ARRAY(point_t), arr) ||
        refl_get_field_size(dyn_option) != sizeof(OPTION_DYN_ARRAY(point_t))) return 13;
    if (fixed_array->kind != K_FIXED_ARRAY ||
        fixed_array->offset != offsetof(OPTION_FIXED_ARRAY(point_t, 2), arr) ||
        fixed_array->size != 2 ||
        refl_get_field_size(fixed_option) != sizeof(OPTION_FIXED_ARRAY(point_t, 2))) return 14;

    point_t source_points[] = { { .x = 1.5f, .y = 2.5f } };
    plain_arrays_t source_arrays = {
        .opt_points = {
            .array = source_points,
            .size = 1,
            .capacity = 1,
            .chunk_size = sizeof(point_t),
        },
        .opt_fixed_points = {
            { .x = 3.5f, .y = 4.5f },
            { .x = 5.5f, .y = 6.5f },
        },
    };
    RStruct* plain_arrays_type = refl_fetch_struct("plain_arrays_t");
    if (!plain_arrays_type) return 20;
    optional_arrays_t* migrated_arrays = refl_migrate_struct(
        plain_arrays_type,
        arrays_type,
        &source_arrays
    );
    if (!migrated_arrays ||
        !migrated_arrays->opt_points.is_some ||
        !migrated_arrays->opt_fixed_points.is_some ||
        ((point_t*)migrated_arrays->opt_points.arr.array)[0].x != 1.5f ||
        migrated_arrays->opt_fixed_points.arr[1].y != 6.5f) return 21;
    plain_arrays_t* migrated_back = refl_migrate_struct(
        arrays_type,
        plain_arrays_type,
        migrated_arrays
    );
    if (!migrated_back ||
        ((point_t*)migrated_back->opt_points.array)[0].y != 2.5f ||
        migrated_back->opt_fixed_points[0].x != 3.5f) return 22;

    const char arrays_input[] =
        "{\"opt_points\":[{\"x\":5.5,\"y\":6.5,\"nested_str\":\"Dynamic string\"}],"
        "\"opt_fixed_points\":[{\"x\":7.5,\"y\":8.5, \"nested_str\": \"Helloooo!\"},{\"x\":9.5,\"y\":10.5}]}";
    DeserializeResult parsed_arrays = deserialize_json(
        arrays_type,
        (void*)arrays_input,
        sizeof(arrays_input) - 1
    );
    if (parsed_arrays.st != DR_OK) return 15;
    optional_arrays_t* array_options = parsed_arrays.data.buff;
    r_array_t* points = &array_options->opt_points.arr;
    if (!array_options->opt_points.is_some ||
        points->size != 1 ||
        points->chunk_size != sizeof(point_t)) return 16;
    point_t* dynamic_points = points->array;
    if (dynamic_points[0].x != 5.5f ||
        dynamic_points[0].y != 6.5f ||
        !dynamic_points[0].nested_str.is_some ||
        strcmp(dynamic_points[0].nested_str.data.data, "Dynamic string")) return 17;
    if (!array_options->opt_fixed_points.is_some ||
        array_options->opt_fixed_points.arr[0].x != 7.5f ||
        array_options->opt_fixed_points.arr[0].y != 8.5f ||
        !array_options->opt_fixed_points.arr[0].nested_str.is_some ||
        strcmp(array_options->opt_fixed_points.arr[0].nested_str.data.data, "Helloooo!") ||
        array_options->opt_fixed_points.arr[1].x != 9.5f ||
        array_options->opt_fixed_points.arr[1].y != 10.5f) return 18;

    void* arrays_json = NULL;
    if (!serialize_json(arrays_type, array_options, &arrays_json, 0) ||
        !strstr(arrays_json, "\"opt_points\"") ||
        !strstr(arrays_json, "\"opt_fixed_points\"") ||
        !strstr(arrays_json, "5.500000") ||
        !strstr(arrays_json, "10.500000")) return 19;
    free(arrays_json);

    refl_generic_destructor(plain_type, without_option);
    refl_generic_destructor(optional_type, with_option);
    refl_generic_destructor(optional_type, parsed_option);
    refl_generic_destructor(arrays_type, array_options);
    refl_generic_destructor(arrays_type, migrated_arrays);
    refl_generic_destructor(plain_arrays_type, migrated_back);
    return 0;
}

#pragma GCC diagnostic pop

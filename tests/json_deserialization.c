#define _XOPEN_SOURCE 500

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <public/core.h>
#include <public/types.h>
#include <public/cmp.h>
#include <public/patch.h>
#include <public/serial/json.h>
#include <public/scope.h>


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

REFLECT(enum) {
    JSON_MODE_IDLE,
    JSON_MODE_ACTIVE,
    JSON_MODE_PAUSED,
} json_mode_t;

REFLECT(struct) {
    bool enabled;
    i32 signed_value;
    u32 unsigned_value;
    double ratio;
    strZ text;
    OPTION(strZ) optional_text;
    OPTION(i32) optional_number;
    json_mode_t mode;
} json_values_t;

static int json_fails_with(
    RStruct* type,
    const char* input,
    size_t input_size,
    DeserializeError expected
) {
    char* mutable_input = malloc(input_size + 1);
    if (!mutable_input) return 0;
    memcpy(mutable_input, input, input_size);
    mutable_input[input_size] = '\0';
    DeserializeResult result = json_deserialize(type, mutable_input, input_size);
    free(mutable_input);
    if (result.st == DR_OK) {
        refl_generic_destructor(type, result.data.buff);
        fprintf(stderr, "Expected JSON error %d for: %s\n", expected, input);
        return 0;
    }
    if (result.err != expected) {
        fprintf(stderr, "Expected JSON error %d, got %d for: %s\n",
            expected, result.err, input);
        return 0;
    }
    return 1;
}

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
    DeserializeResult parsed = json_deserialize(
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

    void* json = NULL;
    if (!json_serialize(optional_type, with_option, &json, 0) ||
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
    DeserializeResult parsed_arrays = json_deserialize(
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
    if (!json_serialize(arrays_type, array_options, &arrays_json, 0) ||
        !strstr(arrays_json, "\"opt_points\"") ||
        !strstr(arrays_json, "\"opt_fixed_points\"") ||
        !strstr(arrays_json, "5.500000") ||
        !strstr(arrays_json, "10.500000")) return 19;
    free(arrays_json);

    RStruct* values_type = refl_fetch_struct("json_values_t");
    if (!values_type) return 23;
    const char values_input[] =
        "{\"enabled\":true,\"signed_value\":-42,\"unsigned_value\":4000000000,"
        "\"ratio\":1.25e2,\"text\":\"quote: \\\" slash: \\\\ line:\\n tab:\\t\","
        "\"optional_text\":null,\"optional_number\":17,\"mode\":\"JSON_MODE_ACTIVE\"}";
    DeserializeResult parsed_values = json_deserialize(
        values_type,
        (void*)values_input,
        sizeof(values_input) - 1
    );
    if (parsed_values.st != DR_OK) {
        fprintf(stderr, "JSON scalar/string parse failed: %s\n",
            deserialize_strerror(parsed_values.err));
        return 24;
    }
    json_values_t* values = parsed_values.data.buff;
    if (!values->enabled ||
        values->signed_value != -42 ||
        values->unsigned_value != 4000000000U ||
        values->ratio != 125.0 ||
        strcmp(values->text.data, "quote: \" slash: \\ line:\n tab:\t") ||
        values->optional_text.is_some ||
        !values->optional_number.is_some ||
        values->optional_number.data != 17 ||
        values->mode != JSON_MODE_ACTIVE) return 25;

    void* values_json = NULL;
    if (!json_serialize(values_type, values, &values_json, 0)) return 26;
    DeserializeResult round_trip = json_deserialize(
        values_type,
        values_json,
        strlen(values_json)
    );
    if (round_trip.st != DR_OK) return 27;
    json_values_t* round_tripped_values = round_trip.data.buff;
    if (!round_tripped_values->enabled ||
        round_tripped_values->signed_value != values->signed_value ||
        round_tripped_values->unsigned_value != values->unsigned_value ||
        round_tripped_values->ratio != values->ratio ||
        strcmp(round_tripped_values->text.data, values->text.data) ||
        round_tripped_values->optional_text.is_some ||
        !round_tripped_values->optional_number.is_some ||
        round_tripped_values->optional_number.data != values->optional_number.data ||
        round_tripped_values->mode != values->mode) return 28;
    free(values_json);

    const char omitted_optionals[] =
        "{\"enabled\":false,\"signed_value\":0,\"unsigned_value\":0,"
        "\"ratio\":0,\"text\":\"\",\"mode\":\"JSON_MODE_IDLE\"}";
    DeserializeResult omitted = json_deserialize(
        values_type,
        (void*)omitted_optionals,
        sizeof(omitted_optionals) - 1
    );
    if (omitted.st != DR_OK) return 29;
    json_values_t* omitted_values = omitted.data.buff;
    if (omitted_values->enabled ||
        omitted_values->optional_text.is_some ||
        omitted_values->optional_number.is_some) return 30;

    const char unknown_nested[] =
        "{\"future\":{\"nested\":[1,{\"value\":true}]},"
        "\"enabled\":false,\"signed_value\":0,\"unsigned_value\":0,"
        "\"ratio\":0,\"text\":\"\",\"mode\":\"JSON_MODE_IDLE\"}";
    DeserializeResult ignored_unknown = json_deserialize(
        values_type,
        (void*)unknown_nested,
        sizeof(unknown_nested) - 1
    );
    if (ignored_unknown.st != DR_OK) return 31;

    if (!json_fails_with(values_type, "", 0, DE_EMPTY_STREAM) ||
        !json_fails_with(values_type, " \t\r\n", 4, DE_EMPTY_STREAM) ||
        !json_fails_with(values_type, "{", 1, DE_UNBALANCED_STREAM) ||
        !json_fails_with(values_type, "[]", 2, DE_STREAM_INCOMPATIBLE) ||
        !json_fails_with(values_type,
            "{\"enabled\":\"true\"}", sizeof("{\"enabled\":\"true\"}") - 1,
            DE_STREAM_INCOMPATIBLE) ||
        !json_fails_with(values_type,
            "{\"enabled\":maybe}", sizeof("{\"enabled\":maybe}") - 1,
            DE_UNEXPECTED_CHARACTER) ||
        !json_fails_with(values_type,
            "{\"enabled\":tru}", sizeof("{\"enabled\":tru}") - 1,
            DE_UNKNOWN_KEYWORD) ||
        !json_fails_with(values_type,
            "{\"enabled\":true,\"signed_value\":0,\"unsigned_value\":0,"
            "\"ratio\":0,\"text\":\"\",\"mode\":\"NO_SUCH_MODE\"}",
            sizeof("{\"enabled\":true,\"signed_value\":0,\"unsigned_value\":0,"
            "\"ratio\":0,\"text\":\"\",\"mode\":\"NO_SUCH_MODE\"}") - 1,
            DE_LABEL_NOT_FOUND) ||
        !json_fails_with(values_type,
            "{\"enabled\":true,\"signed_value\":0,\"unsigned_value\":0,"
            "\"ratio\":0,\"text\":\"bad\\qescape\",\"mode\":\"JSON_MODE_IDLE\"}",
            sizeof("{\"enabled\":true,\"signed_value\":0,\"unsigned_value\":0,"
            "\"ratio\":0,\"text\":\"bad\\qescape\",\"mode\":\"JSON_MODE_IDLE\"}") - 1,
            DE_BADLY_SCAPED_STRING) ||
        !json_fails_with(values_type,
            "{\"enabled\":true,\"signed_value\":0,\"unsigned_value\":0,"
            "\"ratio\":0,\"text\":\"unfinished",
            sizeof("{\"enabled\":true,\"signed_value\":0,\"unsigned_value\":0,"
            "\"ratio\":0,\"text\":\"unfinished") - 1,
            DE_UNCLOSED_DQUOTE) ||
        !json_fails_with(values_type,
            "{\"enabled\":true,\"signed_value\":0,\"unsigned_value\":0,"
            "\"ratio\":0,\"text\":\"\",\"mode\":\"JSON_MODE_IDLE\"} trailing",
            sizeof("{\"enabled\":true,\"signed_value\":0,\"unsigned_value\":0,"
            "\"ratio\":0,\"text\":\"\",\"mode\":\"JSON_MODE_IDLE\"} trailing") - 1,
            DE_EXPECTED_EOF) ||
        !json_fails_with(values_type,
            "{\"enabled\":true,\"signed_value\":0,\"unsigned_value\":0,"
            "\"ratio\":0,\"text\":\"\"}",
            sizeof("{\"enabled\":true,\"signed_value\":0,\"unsigned_value\":0,"
            "\"ratio\":0,\"text\":\"\"}") - 1,
            DE_STREAM_INCOMPATIBLE) ||
        !json_fails_with(values_type,
            "{\"enabled\":false,\"enabled\":true,\"signed_value\":0,"
            "\"unsigned_value\":0,\"ratio\":0,\"text\":\"\"}",
            sizeof("{\"enabled\":false,\"enabled\":true,\"signed_value\":0,"
            "\"unsigned_value\":0,\"ratio\":0,\"text\":\"\"}") - 1,
            DE_STREAM_INCOMPATIBLE) ||
        !json_fails_with(values_type,
            "{\"enabled\":true,\"signed_value\":0,\"unsigned_value\":0,"
            "\"ratio\":0,\"text\":\"\",\"mode\":\"JSON_MODE_IDLE\",}",
            sizeof("{\"enabled\":true,\"signed_value\":0,\"unsigned_value\":0,"
            "\"ratio\":0,\"text\":\"\",\"mode\":\"JSON_MODE_IDLE\",}") - 1,
            DE_UNEXPECTED_CHARACTER)) return 32;

    const char empty_optional_arrays[] =
        "{\"opt_points\":[],\"opt_fixed_points\":[]}";
    refl_generic_destructor(arrays_type, array_options);
    parsed_arrays = json_deserialize(
        arrays_type,
        (void*)empty_optional_arrays,
        sizeof(empty_optional_arrays) - 1
    );
    if (parsed_arrays.st != DR_OK) return 33;
    array_options = parsed_arrays.data.buff;
    if (!array_options->opt_points.is_some ||
        array_options->opt_points.arr.size != 0 ||
        !array_options->opt_fixed_points.is_some) return 34;

    refl_generic_destructor(plain_type, without_option);
    refl_generic_destructor(optional_type, with_option);
    refl_generic_destructor(optional_type, parsed_option);
    refl_generic_destructor(arrays_type, array_options);
    refl_generic_destructor(arrays_type, migrated_arrays);
    refl_generic_destructor(plain_arrays_type, migrated_back);
    refl_generic_destructor(values_type, values);
    refl_generic_destructor(values_type, round_tripped_values);
    refl_generic_destructor(values_type, omitted_values);
    refl_generic_destructor(values_type, ignored_unknown.data.buff);
    printf("JSON tests OK\n");
    return 0;
}

#pragma GCC diagnostic pop

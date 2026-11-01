#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <public/core.h>
#include <public/serial/json.h>
#include <public/types.h>

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

REFLECT(struct) {
    float x;
    float y;
    OPTION(strZ) label;
} json_point_t;

REFLECT(struct) {
    DYN_ARRAY(json_point_t) dynamic_points;
    json_point_t fixed_points[2];
    DYN_ARRAY(i32) empty_numbers;
} json_collections_t;

int main(void) {
    refl_load_metadata(CREFL_FILE);
    refl_threadlocal_init(0);

    RStruct* values_type = refl_fetch_struct("json_values_t");
    RStruct* collections_type = refl_fetch_struct("json_collections_t");
    if (!values_type || !collections_type) return 1;

    json_values_t values = {
        .enabled = true,
        .signed_value = -42,
        .unsigned_value = 4000000000U,
        .ratio = 125.0,
        .text = { .data = "quote: \" slash: \\ line:\n tab:\t" },
        .optional_text = {
            .is_some = true,
            .data = { .data = "optional value" },
        },
        .mode = JSON_MODE_ACTIVE,
    };
    void* output = NULL;
    size_t output_size = json_serialize(values_type, &values, &output, 0);
    const char expected[] =
        "{\"enabled\" : true, \"signed_value\" : -42, "
        "\"unsigned_value\" : 4000000000, \"ratio\" : 125.000000, "
        "\"text\" : \"quote: \\\" slash: \\\\ line:\\n tab:\\t\", "
        "\"optional_text\" : \"optional value\", "
        "\"mode\" : \"JSON_MODE_ACTIVE\"}";
    if (!output || output_size != strlen(expected) ||
        strcmp(output, expected)) {
        fprintf(stderr, "Unexpected JSON serialization: %s\n",
                output ? (char*)output : "(null)");
        free(output);
        return 2;
    }
    free(output);

    json_point_t dynamic_points[] = {
        {
            .x = 1.5f,
            .y = 2.5f,
            .label = {
                .is_some = true,
                .data = { .data = "first" },
            },
        },
        {
            .x = 3.5f,
            .y = 4.5f,
            .label = { .is_some = false },
        },
    };
    json_collections_t collections = {
        .dynamic_points = {
            .array = dynamic_points,
            .size = 2,
            .capacity = 2,
            .chunk_size = sizeof(json_point_t),
        },
        .fixed_points = {
            { .x = 5.5f, .y = 6.5f },
            { .x = 7.5f, .y = 8.5f },
        },
        .empty_numbers = {
            .array = NULL,
            .size = 0,
            .capacity = 0,
            .chunk_size = sizeof(i32),
        },
    };
    output = NULL;
    output_size = json_serialize(collections_type, &collections, &output, 0);
    if (!output || output_size != strlen(output) ||
        !strstr(output,
            "\"dynamic_points\" : [ "
            "{\"x\" : 1.500000, \"y\" : 2.500000, \"label\" : \"first\"}, "
            "{\"x\" : 3.500000, \"y\" : 4.500000} ]") ||
        !strstr(output,
            "\"fixed_points\" : [ "
            "{\"x\" : 5.500000, \"y\" : 6.500000}, "
            "{\"x\" : 7.500000, \"y\" : 8.500000} ]") ||
        !strstr(output, "\"empty_numbers\" : [  ]")) {
        fprintf(stderr, "Unexpected JSON collection serialization: %s\n",
                output ? (char*)output : "(null)");
        free(output);
        return 3;
    }
    free(output);

    puts("JSON serialization tests OK");
    return 0;
}

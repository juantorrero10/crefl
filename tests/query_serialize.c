#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <public/core.h>
#include <public/serial/json.h>
#include <public/serial/query.h>
#include <public/types.h>

REFLECT(enum) {
    QUERY_COLOR_RED = 3,
    QUERY_COLOR_GREEN = 7,
    QUERY_COLOR_BLUE = 19
} query_color_t;
DECL_OPTION(query_color_t);

REFLECT(struct) {
    strZ name;
    OPTION(strZ) optional;
    strZ null_value;
    strZ fixed[2];
    DYN_ARRAY(strZ) tags;
    i32 count;
    query_color_t color;
} query_serialize_test_t;

int main(void) {
    refl_load_metadata(CREFL_FILE);
    refl_threadlocal_init(0);

    RStruct* model = refl_fetch_struct("query_serialize_test_t");
    if (!model) return 1;

    strZ tags[] = {
        { .data = "fast" },
        { .data = "safe" },
    };
    query_serialize_test_t value = {
        .name = { .data = "A B&+._-~%\"line\nnext" },
        .optional = { .is_some = false },
        .fixed = {
            { .data = "red" },
            { .data = "blue" },
        },
        .tags = {
            .array = tags,
            .size = 2,
            .capacity = 2,
            .chunk_size = sizeof(strZ),
        },
        .count = -42,
        .color = QUERY_COLOR_GREEN,
    };

    void* output = NULL;
    size_t output_size = query_serialize(model, &value, &output);
    const char expected[] =
        "name=A%20B%26+._-%7E%25%22line%0Anext&null_value=null"
        "&fixed=red&fixed=blue&tags=fast&tags=safe"
        "&count=-42&color=QUERY_COLOR_GREEN";
    if (!output || output_size != strlen(expected) ||
        strcmp(output, expected)) {
        fprintf(stderr, "Unexpected query serialization: %s\n",
                output ? (char*)output : "(null)");
        free(output);
        return 2;
    }
    free(output);

    value.optional = (OPTION(strZ)){
        .is_some = true,
        .data = { .data = "optional value" },
    };
    value.tags.size = 0;
    output = NULL;
    output_size = query_serialize(model, &value, &output);
    const char optional_expected[] =
        "name=A%20B%26+._-%7E%25%22line%0Anext"
        "&optional=optional%20value&null_value=null"
        "&fixed=red&fixed=blue&count=-42&color=QUERY_COLOR_GREEN";
    if (!output || output_size != strlen(optional_expected) ||
        strcmp(output, optional_expected)) {
        fprintf(stderr, "Unexpected query serialization with optional: %s\n",
                output ? (char*)output : "(null)");
        free(output);
        return 3;
    }
    free(output);

    void* json_output = NULL;
    if (!json_serialize(model, &value, &json_output, 0) ||
        !json_output ||
        !strstr(json_output, "\"fixed\" : [ \"red\", \"blue\" ]") ||
        !strstr(json_output, "\"tags\" : [  ]") ||
        !strstr(json_output, "\"null_value\" : \"\"")) {
        free(json_output);
        return 4;
    }
    free(json_output);

    puts("OK");
    return 0;
}

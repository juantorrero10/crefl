#!/usr/bin/env bash

cd "$(dirname "$0")/.." || exit 1

failed=0

run_test() {
    local name=$1
    shift

    local output
    output=$("$@" 2>&1)
    local code=$?

    if [ "$code" -eq 0 ]; then
        printf '%s...   \033[32mOK\033[0m\n' "$name"
    else
        printf '%s...   \033[31mERR(%s)\033[0m\n' "$name" "$code"
        printf '%s\n' "$output" >&2
        failed=1
    fi
}

run_test test_option_macros python3 tests/test_option_macros.py
run_test test_query_deserialize make test_query_deserialize
run_test test_query_serialize make test_query_serialize
run_test test_json_deserialize make test_json_deserialize
run_test test_json_serialize make test_json_serialize

valgrind_args=(
    --quiet
    --leak-check=full
    --show-leak-kinds=all
    --errors-for-leak-kinds=definite,indirect,possible
    --error-exitcode=1
)
run_test memcheck_query_deserialize valgrind "${valgrind_args[@]}" \
    build/bin/query_deserialization
run_test memcheck_query_serialize valgrind "${valgrind_args[@]}" \
    build/bin/query_serialize
run_test memcheck_json_deserialize valgrind "${valgrind_args[@]}" \
    build/bin/json_deserialization
run_test memcheck_json_serialize valgrind "${valgrind_args[@]}" \
    build/bin/json_serialize

exit "$failed"

#!/usr/bin/env bash

cd "$(dirname "$0")/.." || exit 1

failed=0

run_test() {
    local name=$1
    shift

    "$@" >/dev/null 2>&1
    local code=$?

    if [ "$code" -eq 0 ]; then
        printf '%s...   \033[32mOK\033[0m\n' "$name"
    else
        printf '%s...   \033[31mERR(%s)\033[0m\n' "$name" "$code"
        failed=1
    fi
}

run_test test_option_macros python3 tests/test_option_macros.py
run_test test_query_deserialize make test_query_deserialize
run_test test_query_serialize make test_query_serialize
run_test test_json make test_json

exit "$failed"

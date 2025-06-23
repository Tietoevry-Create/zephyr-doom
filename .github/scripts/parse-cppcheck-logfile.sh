#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR=$(realpath "$(dirname "$0")")
# shellcheck disable=SC1091
source "${SCRIPT_DIR}/config.sh"

usage() {
    echo "Usage: ./$(basename "$0") [-h] <input_log_file>"
}

args_help() {
    echo "Mandatory arguments:
    input_log_file    Path to the Cppcheck log file to parse."

    echo

    echo "Optional arguments:
    -h      Show help and exit."
}

while getopts ":h" arg; do
    case "$arg" in
        h) usage; echo; args_help; exit ;;
        *) usage; exit ;;
    esac
done

INPUT_LOG_FILE="$1"
if [[ -z "$INPUT_LOG_FILE" ]]; then
    echo "Please provide an input log file path!" >&2
    usage
    exit 1
fi

LOG_FILE_PATH=$(realpath -e "$INPUT_LOG_FILE")
OUTPUT_LOG_FILE_PATH="$(basename "${LOG_FILE_PATH%.log}")-summary.log"

EXIT_CODE=0

ISSUE_KEYS=(ERROR NOTE WARNING STYLE PERFORMANCE PORTABILITY)

declare -A ISSUE_COUNT_ARRAY
for key in "${ISSUE_KEYS[@]}"; do
    ISSUE_COUNT_ARRAY["$key"]=0
done
ISSUE_COUNT_ARRAY[TOTAL]=0

declare -A ISSUE_THRESHOLD_ARRAY
for key in "${ISSUE_KEYS[@]}"; do
    var_name="${key}_THRESHOLD"
    ISSUE_THRESHOLD_ARRAY["$key"]="${!var_name}"
done

echo -e "Parsing Cppcheck log file - $(basename "$LOG_FILE_PATH") ...\n" 2>&1 | tee "$OUTPUT_LOG_FILE_PATH"

for key in "${ISSUE_KEYS[@]}"; do
    lower_key=$(echo "$key" | tr '[:upper:]' '[:lower:]')
    count=$(grep -c "$lower_key:" "$LOG_FILE_PATH" || true)
    ISSUE_COUNT_ARRAY["$key"]=${count:-0}

    if (( "${ISSUE_COUNT_ARRAY["$key"]}" >= "${ISSUE_THRESHOLD_ARRAY["$key"]}" )); then
        echo -e "[FAIL] ${lower_key} issues: ${ISSUE_COUNT_ARRAY[$key]} (threshold: ${ISSUE_THRESHOLD_ARRAY[$key]}) ...\n" 2>&1 | tee -a "$OUTPUT_LOG_FILE_PATH"

        EXIT_CODE=1
    fi
done

{
    echo "Issues Overview"
    echo "==================================="

    for key in "${ISSUE_KEYS[@]}"; do
        printf "%-12s: %-4s (threshold: %s)\n" \
            "$key" \
            "${ISSUE_COUNT_ARRAY[$key]}" \
            "${ISSUE_THRESHOLD_ARRAY[$key]}"

        (( ISSUE_COUNT_ARRAY[TOTAL] += ISSUE_COUNT_ARRAY["$key"] ))
    done

    echo "-----------------------------------"
    printf "%-12s: %s\n" "TOTAL" "${ISSUE_COUNT_ARRAY[TOTAL]}"
} 2>&1 | tee -a "$OUTPUT_LOG_FILE_PATH"

echo
echo "Output written to - $OUTPUT_LOG_FILE_PATH ..."

exit "$EXIT_CODE"

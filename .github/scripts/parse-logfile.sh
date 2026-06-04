#!/usr/bin/env bash

set -eo pipefail

SCRIPT_DIR=$(realpath "$(dirname "$0")")
# shellcheck disable=SC1091
source "${SCRIPT_DIR}/config.sh"

usage() {
    echo "Usage: ./$(basename "$0") [-h] [-m mode] <input_log_file>"
}

args_help() {
    echo "Mandatory arguments:
    -m                Specifies the mode the script should run in.
                      Supported modes: ${HANDLED_MODES[*]}.

    input_log_file    Path to the Cppcheck log file to parse."

    echo

    echo "Optional arguments:
    -h      Show help and exit."
}

while getopts "m:h" arg; do
    if [[ "${OPTARG}" == -* ]]; then
        echo "${arg} option needs an argument!" >&2
        usage
        exit 1
    fi

    case "$arg" in
        m) MODE="${OPTARG}" ;;
        h) usage; echo; args_help; exit ;;
        *) usage; exit ;;
    esac
done

if [[ -z "$MODE" ]]; then
    echo "Please provide script mode for further execution!" >&2
    usage
    exit 1
else
    if [[ ! "${HANDLED_MODES[*]}" == *"$MODE"* ]]; then
        echo "The mode '$MODE' is not handled by the script." >&2
        exit 1
    fi
fi

shift $((OPTIND - 1))

INPUT_LOG_FILE="$1"
if [[ -z "$INPUT_LOG_FILE" ]]; then
    echo "Please provide an input log file path!" >&2
    usage
    exit 1
fi

LOG_FILE_PATH=$(realpath -e "$INPUT_LOG_FILE")
OUTPUT_LOG_FILE_PATH="$(basename "${LOG_FILE_PATH%.log}")-summary.log"

EXIT_CODE=0

case "$MODE" in
    cppcheck)
        ISSUE_KEYS=(ERROR NOTE PERFORMANCE PORTABILITY STYLE WARNING) ;;
    pylint)
        ISSUE_KEYS=(CONVENTION ERROR FATAL INFO REFACTOR WARNING) ;;
    flake8)
        ISSUE_KEYS=(COMPLEXITY ERROR FATAL WARNING) ;;
esac

declare -A ISSUE_COUNT_ARRAY
for key in "${ISSUE_KEYS[@]}"; do
    ISSUE_COUNT_ARRAY["$key"]=0
done
ISSUE_COUNT_ARRAY[TOTAL]=0

declare -A ISSUE_THRESHOLD_ARRAY
for key in "${ISSUE_KEYS[@]}"; do
    case "$MODE" in
        cppcheck)
            prefix=CPPCHECK ;;
        pylint)
            prefix=PYLINT ;;
        flake8)
            prefix=FLAKE8 ;;
    esac
    var_name="${prefix}_${key}_THRESHOLD"
    ISSUE_THRESHOLD_ARRAY["$key"]="${!var_name}"
done

echo -e "Parsing $MODE log file - $(basename "$LOG_FILE_PATH") ...\n" 2>&1 | tee "$OUTPUT_LOG_FILE_PATH"

for key in "${ISSUE_KEYS[@]}"; do
    case "$MODE" in
        cppcheck)
            mod_key=$(echo "$key" | tr '[:upper:]' '[:lower:]')
            regexp="${mod_key}:"
            ;;
        pylint)
            mod_key="${key:0:1}"
            regexp="${mod_key}[0-9]+:"
            ;;
        flake8)
            mod_key="${key:0:1}"
            regexp=":[[:space:]]${mod_key}[0-9]+"
            ;;
    esac
    count=$(grep -Ec "$regexp" "$LOG_FILE_PATH" || true)
    ISSUE_COUNT_ARRAY["$key"]=${count:-0}

    if (( "${ISSUE_COUNT_ARRAY["$key"]}" >= "${ISSUE_THRESHOLD_ARRAY["$key"]}" )); then
        echo -e "[FAIL] ${key} issues: ${ISSUE_COUNT_ARRAY[$key]} (threshold: ${ISSUE_THRESHOLD_ARRAY[$key]}) ...\n" 2>&1 | tee -a "$OUTPUT_LOG_FILE_PATH"

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

        (( ISSUE_COUNT_ARRAY[TOTAL] += ISSUE_COUNT_ARRAY["$key"] )) || :
    done

    echo "-----------------------------------"
    printf "%-12s: %s\n" "TOTAL" "${ISSUE_COUNT_ARRAY[TOTAL]}"
} 2>&1 | tee -a "$OUTPUT_LOG_FILE_PATH"

echo
echo "Output written to - $OUTPUT_LOG_FILE_PATH ..."

exit "$EXIT_CODE"

#!/usr/bin/env bash

set -eo pipefail

usage() {
cat << EOF
Usage: ./$(basename "$0") [-h] [-t token]
EOF
}

args_help() {
    echo "Mandatory arguments:
    -t    Token used to authenticate the removal request."

    echo

    echo "Optional arguments:
    -h    Show help and exit."
}

while getopts "t:h" arg; do
    if [[ "${OPTARG}" == -* ]]; then
        echo "${arg} option needs an argument!" >&2
        usage
        exit 1
    fi

    case "$arg" in
        t) TOKEN="${OPTARG}" ;;
        h) usage; echo; args_help; exit ;;
        *) usage; exit ;;
    esac
done

if [[ -z "$TOKEN" ]]; then
    echo "Please provide an authentication token!" >&2
    usage
    exit 1
fi

RUNNER_DIR_PATH="$HOME/actions-runner"

"$RUNNER_DIR_PATH/config.sh" remove --token "$TOKEN"

rm -rf "$RUNNER_DIR_PATH"

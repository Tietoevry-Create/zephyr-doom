#!/usr/bin/env bash

set -euo pipefail

usage() {
    echo "Usage: ./$(basename "$0") [-h]"
}

args_help() {
    echo "Optional arguments:
    -h      Show help and exit."
}

while getopts ":h" arg; do
    case "$arg" in
        h) usage; echo; args_help; exit ;;
        *) usage; exit ;;
    esac
done

RUNNER_DIR_PATH="$HOME/actions-runner"

"$RUNNER_DIR_PATH/run.sh"

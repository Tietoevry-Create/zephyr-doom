#!/usr/bin/env bash

set -eo pipefail

usage() {
cat << EOF
Usage: ./$(basename "$0") [-h] [-r repo_url] [-t TOKEN]
EOF
}

args_help() {
    echo "Mandatory arguments:
    -r    GitHub repository URL for which the runner should be configured.
    -t    Token used to authenticate the configuration request."

    echo

    echo "Optional arguments:
    -h    Show help and exit."
}

while getopts "r:t:h" arg; do
    case "$arg" in
        r) REPO_URL="${OPTARG}" ;;
        t) TOKEN="${OPTARG}" ;;
        h) usage; echo; args_help; exit ;;
        *) usage; exit ;;
    esac
done

if [[ -z "$REPO_URL" ]]; then
    echo "Please provide a GitHub repository URL!" >&2
    usage
    exit 1
fi

if [[ -z "$TOKEN" ]]; then
    echo "Please provide an authentication token!" >&2
    usage
    exit 1
fi

RUNNER_DIR_PATH="$HOME/actions-runner"

"$RUNNER_DIR_PATH/config.sh" --unattended --url "$REPO_URL" --token "$TOKEN"

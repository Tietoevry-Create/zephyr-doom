#!/usr/bin/env bash

set -eo pipefail

usage() {
cat << EOF
Usage: ./$(basename "$0") [-h] [-r repo_url] [-t token] [-n name] [-l labels]
EOF
}

args_help() {
    echo "Mandatory arguments:
    -r    GitHub repository URL for which the runner should be configured.
    -t    Token used to authenticate the configuration request."

    echo

    echo "Optional arguments:
    -n    Specify a name for the runner. By default set to hostname.
    -l    Add custom labels to the runner configuration.
          Defaults to the hostname.
    -h    Show help and exit."
}

# Default values
NAME=$(hostname)
LABELS="$NAME"

while getopts "r:t:n:l:h" arg; do
    case "$arg" in
        r) REPO_URL="${OPTARG}" ;;
        t) TOKEN="${OPTARG}" ;;
        n) NAME="${OPTARG}" ;;
        l) LABELS="${OPTARG}" ;;
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

"$RUNNER_DIR_PATH/config.sh" --unattended --url "$REPO_URL" --token "$TOKEN" \
                             --name "$NAME" --labels "$LABELS"

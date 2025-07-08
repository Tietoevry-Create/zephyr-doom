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

podman kill gha-runner || true
podman rm -fiv gha-runner
podman run --rm --name=gha-runner gha-runner-image:latest

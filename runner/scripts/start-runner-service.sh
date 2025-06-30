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

RUNNER_REPOSITORY_DIR_PATH="$HOME/zephyr-doom/runner/systemd"
SERVICE_DIR_PATH="$HOME/.config/systemd/user"
SERVICE_FILE_NAME=github-actions-runner.service

mkdir -p "$SERVICE_DIR_PATH"
cd "$SERVICE_DIR_PATH" && \
ln -sf "${RUNNER_REPOSITORY_DIR_PATH}/${SERVICE_FILE_NAME}" "$SERVICE_FILE_NAME"

systemctl --user daemon-reload
systemctl --user --now enable "$SERVICE_FILE_NAME"

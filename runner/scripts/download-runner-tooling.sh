#!/usr/bin/env bash

set -eo pipefail

usage() {
cat << EOF
Usage: ./$(basename "$0") [-h] [-a architecture] [-v version] [-s hash]
EOF
}

args_help() {
    echo "Mandatory arguments:
    -a    Architecture of the GitHub actions runner to download.
    -v    Version of the GitHub Actions runner tooling to download."

    echo

    echo "Optional arguments:
    -s    Checksum hash to verify the downloaded archive.
    -h    Show help and exit."
}

while getopts "a:v:s:h" arg; do
    if [[ "${OPTARG}" == -* ]]; then
        echo "${arg} option needs an argument!" >&2
        usage
        exit 1
    fi

    case "$arg" in
        a) ARCH="${OPTARG}" ;;
        v) VERSION="${OPTARG}" ;;
        s) HASH="${OPTARG}" ;;
        h) usage; echo; args_help; exit ;;
        *) usage; exit ;;
    esac
done

if [[ -z "$ARCH" ]]; then
    echo "Please provide an architecture to download!" >&2
    usage
    exit 1
fi

if [[ -z "$VERSION" ]]; then
    echo "Please provide an tooling version to download!" >&2
    usage
    exit 1
fi

RUNNER_DIR_PATH="$HOME/actions-runner"
mkdir "$RUNNER_DIR_PATH" && cd "$RUNNER_DIR_PATH"

curl -o "actions-runner-linux-${ARCH}-${VERSION}.tar.gz" \
-L "https://github.com/actions/runner/releases/download/v${VERSION}/actions-runner-linux-${ARCH}-${VERSION}.tar.gz"

if [[ -n "$HASH" ]]; then
    echo "$HASH  actions-runner-linux-${ARCH}-${VERSION}.tar.gz" | \
    sha256sum -c
fi

tar -xzf "actions-runner-linux-${ARCH}-${VERSION}.tar.gz"

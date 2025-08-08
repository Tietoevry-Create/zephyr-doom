#!/usr/bin/env bash

set -eo pipefail

usage() {
cat << EOF
Usage: ./$(basename "$0") [-h] [-a architecture] [-v version] [-s hash]
                          [-r repo_url] [-t token] [-n name] [-l labels]
EOF
}

args_help() {
    echo "Mandatory arguments:
    -a    Architecture of the GitHub actions runner to download.
    -v    Version of the GitHub Actions runner tooling to download.
    -s    Checksum hash to verify the downloaded archive.
    -r    GitHub repository URL for which the runner should be configured.
    -t    Token used to authenticate the configuration request."

    echo

    echo "Optional arguments:
    -n    Specify a name for the runner. By default set to hostname of container
          host with prefix.
    -l    Add custom labels to the runner configuration.
          Defaults to the hostname of container host with prefix.
    -h    Show help and exit."
}

NAME="container-$(hostname)"
LABELS="$NAME"

while getopts "a:v:s:r:t:n:l:h" arg; do
    case "$arg" in
        a) ARCH="${OPTARG}" ;;
        v) VERSION="${OPTARG}" ;;
        s) HASH="${OPTARG}" ;;
        r) REPO_URL="${OPTARG}" ;;
        t) TOKEN="${OPTARG}" ;;
        n) NAME="${OPTARG}" ;;
        l) LABELS="${OPTARG}" ;;
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

if [[ -z "$HASH" ]]; then
    echo "Please provide checksum hash to verify the downloaded archive!" >&2
    usage
    exit 1
fi

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

export TOKEN="$TOKEN"

podman build \
--build-arg ARCH="$ARCH" \
--build-arg VERSION="$VERSION" \
--build-arg HASH="$HASH" \
--build-arg REPO_URL="$REPO_URL" \
--build-arg NAME="$NAME" \
--build-arg LABELS="$LABELS" \
--secret type=env,id=token,env=TOKEN \
-t gha-runner-image .

unset TOKEN

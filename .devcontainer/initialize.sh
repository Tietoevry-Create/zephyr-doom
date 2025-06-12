#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR_PATH=$(realpath "$(dirname "$0")")
GIT_ENV_PATH="$SCRIPT_DIR_PATH/git.env"

if [[ ! -f "$GIT_ENV_PATH" ]]; then
    if [[ -z "$GIT_AUTHOR_NAME" || -z "$GIT_AUTHOR_EMAIL" ]]; then
        echo "GIT_AUTHOR_NAME and/or GIT_AUTHOR_EMAIL is not set."
        echo "Please set it to properly configure the environment."
    fi

    {
        echo "GIT_AUTHOR_NAME=\"$GIT_AUTHOR_NAME\""
        echo "GIT_AUTHOR_EMAIL=\"$GIT_AUTHOR_EMAIL\""
        echo "GIT_COMMITTER_NAME=\"$GIT_AUTHOR_NAME\""
        echo "GIT_COMMITTER_EMAIL=\"$GIT_AUTHOR_EMAIL\""
    } > "$GIT_ENV_PATH"
fi

chmod 600 "$GIT_ENV_PATH"

echo "Finished execution of $(basename "${0}") ..."

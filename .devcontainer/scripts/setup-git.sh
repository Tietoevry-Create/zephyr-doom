#!/usr/bin/env bash

set -euo pipefail

GIT_CONFIG_TEMPLATE_PATH=/workspaces/zephyr-doom/.devcontainer/templates/.gitconfig.template
GIT_CONFIG_TARGET_PATH=/workspaces/zephyr-doom/.devcontainer/.gitconfig
GIT_CONFIG_HOME_PATH="$HOME/.gitconfig"

#Configure basic git configuration
if [[ ! -f "$GIT_CONFIG_HOME_PATH" ]]; then
    cp "$GIT_CONFIG_TEMPLATE_PATH" "$GIT_CONFIG_TARGET_PATH"
    chmod 600 "$GIT_CONFIG_TARGET_PATH"

    sed -i "s/TPL_GIT_EMAIL/$GIT_AUTHOR_EMAIL/g" "$GIT_CONFIG_TARGET_PATH"
    sed -i "s/TPL_GIT_USERNAME/$GIT_AUTHOR_NAME/g" "$GIT_CONFIG_TARGET_PATH"

    ln -sf "$GIT_CONFIG_TARGET_PATH" "$GIT_CONFIG_HOME_PATH"
fi

# Install pre-commit hooks
pre-commit install

echo "Finished execution of $(basename "${0}") ..."

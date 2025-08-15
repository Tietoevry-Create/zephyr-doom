#!/usr/bin/env bash

set -euo pipefail

TOOLCHAIN_VERSION=v2.6.2
SDK_VERSION=v2.6.0

SDK_DIR_PATH="$HOME/ncs/$SDK_VERSION"

# Install nRF toolchain and SDK
if [[ ! -d "$SDK_DIR_PATH" ||
      -z $(ls -A "$SDK_DIR_PATH") ]]; then
    nrfutil self-upgrade
    nrfutil install device
    nrfutil install toolchain-manager
    nrfutil toolchain-manager install --ncs-version "$TOOLCHAIN_VERSION"
    nrfutil install sdk-manager
    nrfutil sdk-manager install "$SDK_VERSION"
fi

echo "Finished execution of $(basename "${0}") ..."

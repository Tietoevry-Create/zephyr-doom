#!/usr/bin/env bash

set -euo pipefail

TOOLCHAIN_VERSION=v2.6.2
SDK_VERSION=v2.6.0
JLINK_PKG_NAME="JLink_Linux_V794e_x86_64.deb"

SDK_DIR_PATH="$HOME/ncs/$SDK_VERSION"
JLINK_PKG_PATH="/workspaces/zephyr-doom/.devcontainer/${JLINK_PKG_NAME}"
DPKG_INFO_PATH="/var/lib/dpkg/info"

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

# Install SEGGER J-Link
if ! dpkg -s jlink &>/dev/null; then
  sudo dpkg --unpack "$JLINK_PKG_PATH"
  sudo rm -f "${DPKG_INFO_PATH}/jlink.postinst"
  sudo dpkg --force-all --configure jlink
  sudo apt-get -y install -f
fi

echo "Finished execution of $(basename "${0}") ..."

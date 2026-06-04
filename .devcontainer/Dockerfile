FROM docker.io/ubuntu:24.04

# Install common packages
RUN apt-get update && apt-get -y install \
    bash-completion \
    ca-certificates \
    curl \
    git \
    gnupg \
    htop \
    man \
    patch \
    rsync \
    sudo \
    tar \
    tree \
    vim \
    wget

# Install zephyr-doom project specific packages
RUN apt-get update && apt-get -y install \
    cmake \
    cppcheck \
    g++ \
    gcc \
    libunistring5 \
    libusb-1.0-0 \
    make \
    pre-commit

# Install Python packages
RUN apt-get update && apt-get -y install \
    python3 \
    python3-dev \
    python3-pip \
    python3-setuptools \
    python3-venv

# Install Docker packages
RUN install -m 0755 -d /etc/apt/keyrings
RUN curl -fsSL https://download.docker.com/linux/ubuntu/gpg -o /etc/apt/keyrings/docker.asc
RUN chmod a+r /etc/apt/keyrings/docker.asc

RUN echo \
    "deb [arch=$(dpkg --print-architecture) signed-by=/etc/apt/keyrings/docker.asc] https://download.docker.com/linux/ubuntu \
    $(. /etc/os-release && echo "${UBUNTU_CODENAME:-$VERSION_CODENAME}") stable" | \
    tee /etc/apt/sources.list.d/docker.list > /dev/null

RUN apt-get update && apt-get -y install \
    containerd.io \
    docker-buildx-plugin \
    docker-ce \
    docker-ce-cli \
    docker-compose-plugin

# Link libunistring library to avoid nrfutil runtime errors
RUN cd /lib/x86_64-linux-gnu/ && ln -sf libunistring.so.5 libunistring.so.2

# Install nRF Command Line Tools for backward compatibility
RUN curl https://nsscprodmedia.blob.core.windows.net/prod/software-and-other-downloads/desktop-software/nrf-command-line-tools/sw/versions-10-x-x/10-24-2/nrf-command-line-tools_10.24.2_amd64.deb \
    -o /tmp/nrf-command-line-tools_10.24.2_amd64.deb && \
    sudo apt-get -y install /tmp/nrf-command-line-tools_10.24.2_amd64.deb && \
    rm -rf /tmp/nrf-command-line-tools_10.24.2_amd64.deb

# Install SEGGER J-Link tool (delivered with nRF Command Line Tools).
# Exclude the udevadm configuration from the J-Link package,
# because the environment is not compatible.
RUN sudo dpkg --unpack /opt/nrf-command-line-tools/share/JLink_Linux_V794e_x86_64.deb && \
    sudo rm -f "/var/lib/dpkg/info/jlink.postinst" && \
    sudo dpkg --force-all --configure jlink && \
    sudo apt-get -y install -f

# Install nrfutil tool
RUN curl https://files.nordicsemi.com/artifactory/swtools/external/nrfutil/executables/x86_64-unknown-linux-gnu/nrfutil \
    -o /usr/local/bin/nrfutil && \
    chmod +x /usr/local/bin/nrfutil

# Set the timezone to CET
RUN ln -sf /usr/share/zoneinfo/CET /etc/localtime && \
    echo "CET" > /etc/timezone

# Check for any other system updates
RUN apt-get update && apt-get -y upgrade

# Clean the installation process
RUN apt-get -y autoremove && apt-get autoclean && apt-get clean

# Create the developer user and grant sudo privileges
ARG USERNAME=developer
ARG USER_UID=1000
ARG USER_GID=$USER_UID

RUN userdel -rf ubuntu && \
    groupadd -g "$USER_GID" "$USERNAME" && \
    useradd -u "$USER_UID" -g "$USER_GID" -s /bin/bash -m "$USERNAME" && \
    echo "$USERNAME ALL=(root) NOPASSWD:ALL" > "/etc/sudoers.d/$USERNAME" && \
    chmod 440 "/etc/sudoers.d/$USERNAME"

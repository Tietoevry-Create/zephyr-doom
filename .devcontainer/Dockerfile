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
RUN cd /lib/x86_64-linux-gnu/ && ln -sf libunistring.so.5.0.0 libunistring.so.2

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
    groupadd -g "$USER_GID" "$USERNAME" \
    && useradd -u "$USER_UID" -g "$USER_GID" -s /bin/bash -m "$USERNAME" \
    && echo "$USERNAME ALL=(root) NOPASSWD:ALL" > "/etc/sudoers.d/$USERNAME" \
    && chmod 440 "/etc/sudoers.d/$USERNAME"

FROM docker.io/ubuntu:24.04

# Install common packages
RUN apt-get update && apt-get -y install \
    bash-completion \
    ca-certificates \
    curl \
    htop \
    man \
    sudo \
    tar \
    tree \
    vim \
    wget

# Install required packages
RUN apt-get update && apt-get install -y \
    libkrb5-3 \
    liblttng-ust1 \
    zlib1g \
    $(apt-cache search '^libicu[0-9][0-9]$' | \
    awk '{print $1}' | \
    sort -r | \
    head -n 1) \
    $(apt-cache search '^libssl[0-9]$' | \
    awk '{print $1}' | \
    sort -r | \
    head -n 1)

# Check for any other system updates
RUN apt-get update && apt-get -y upgrade

# Clean the installation process
RUN apt-get -y autoremove && apt-get autoclean && apt-get clean

# Set the timezone to CET
RUN ln -sf /usr/share/zoneinfo/CET /etc/localtime && \
    echo "CET" > /etc/timezone

# Remove the default ubuntu user and create a runner user
ARG USERNAME=runner
RUN userdel -rf ubuntu && useradd -s /bin/bash -m "$USERNAME" && \
    echo "$USERNAME ALL=(root) NOPASSWD:ALL" > "/etc/sudoers.d/$USERNAME" && \
    chmod 440 "/etc/sudoers.d/$USERNAME"

# Set the default user and working directory
USER "$USERNAME"
WORKDIR "/home/$USERNAME"

# Download runner tooling
ARG ARCH
ARG VERSION
ARG HASH
COPY ./download-runner-tooling.sh download-runner-tooling.sh
RUN ./download-runner-tooling.sh -a "$ARCH" -v "$VERSION" -s "$HASH"

# Configure runner environment
ARG REPO_URL
ARG NAME
ARG LABELS
COPY ./configure-runner-env.sh configure-runner-env.sh
RUN --mount=type=secret,id=token,mode=0444 \
    ./configure-runner-env.sh -r "$REPO_URL" -t "$(cat /run/secrets/token)" \
    -n "$NAME" -l "$LABELS"

# Start runner on container start
COPY ./start-runner.sh start-runner.sh
ENTRYPOINT [ "./start-runner.sh" ]

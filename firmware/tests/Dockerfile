# Build and host-test environment for the Atrium pod firmware (Zephyr v4.4.2, Zephyr SDK 1.0.1, ARM toolchain only).
# Works on x86_64 and aarch64 hosts. Build context is firmware/: see firmware/README.md.
FROM ubuntu:24.04

ARG SDK_VER=1.0.1
ENV DEBIAN_FRONTEND=noninteractive LANG=C.UTF-8

RUN apt-get update && apt-get install -y --no-install-recommends \
      git cmake ninja-build gperf ccache device-tree-compiler wget xz-utils file make \
      gcc g++ libc6-dev python3 python3-venv python3-dev ca-certificates \
    && rm -rf /var/lib/apt/lists/*

RUN python3 -m venv /opt/venv
ENV PATH=/opt/venv/bin:$PATH
RUN pip install --no-cache-dir west

RUN ARCH=$(uname -m) && cd /opt \
    && wget -q https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v${SDK_VER}/zephyr-sdk-${SDK_VER}_linux-${ARCH}_minimal.tar.xz \
    && tar xf zephyr-sdk-${SDK_VER}_linux-${ARCH}_minimal.tar.xz && rm zephyr-sdk-*.tar.xz

RUN /opt/zephyr-sdk-${SDK_VER}/setup.sh -t arm-zephyr-eabi -h -c
ENV ZEPHYR_SDK_INSTALL_DIR=/opt/zephyr-sdk-${SDK_VER}

# Zephyr workspace from firmware/west.yml, shallow clones only
COPY src/west.yml /zws/manifest/west.yml
RUN cd /zws && git init -q manifest && west init -l manifest \
    && west update --narrow -o=--depth=1 \
    && pip install --no-cache-dir -r zephyr/scripts/requirements-base.txt -r zephyr/scripts/requirements-build-test.txt -r zephyr/scripts/requirements-run-test.txt
RUN pip install --no-cache-dir -r /zws/bootloader/mcuboot/scripts/requirements.txt
ENV ZEPHYR_BASE=/zws/zephyr
WORKDIR /work

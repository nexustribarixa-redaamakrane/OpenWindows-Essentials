ARG BASE_IMAGE=docker.io/debian
ARG BASE_IMAGE_TAG=trixie-slim
FROM ${BASE_IMAGE}:${BASE_IMAGE_TAG} AS base-image

ENV APT_UPDATE="apt-get update" \
    APT_INSTALL="env DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends" \
    APT_CLEANUP="rm -rf /var/lib/apt/lists/* /var/cache/apt/archives/*"

FROM base-image AS base

LABEL org.opencontainers.image.title="Pixman build environment for platform coverage" \
      org.opencontainers.image.authors="Marek Pikuła <m.pikula@partner.samsung.com>"

ARG GCOVR_VERSION="~=7.2"
ARG MESON_VERSION="~=1.7"
RUN . /etc/os-release \
    && echo "deb http://deb.debian.org/debian ${VERSION_CODENAME}-backports main" > /etc/apt/sources.list.d/backports.list \
    && ${APT_UPDATE} \
    && ${APT_INSTALL} \
        # Build dependencies.
        build-essential \
        ninja-build \
        pkg-config \
        # pipx dependencies.
        python3-venv \
        # gcovr dependencies.
        python3-lxml \
    # User bookworm-backports for QEMU, as it has version 7 by default, which
    # has some issues.
    && ${APT_INSTALL} \
        $(grep bookworm /etc/os-release >/dev/null && echo -t bookworm-backports) \
        qemu-user \
    && ${APT_CLEANUP} \
    # Install pipx using pip to have a more recent version of pipx, which
    # supports the `--global` flag. Use a dedicated venv, as recent pipx
    # requires newer dependencies than Debian provides, and pip cannot
    # replace Debian-installed packages.
    && python3 -m venv /opt/pipx-venv \
    && /opt/pipx-venv/bin/pip install pipx \
    && ln -s /opt/pipx-venv/bin/pipx /usr/local/bin/pipx \
    # Install a recent version of meson and gcovr using pipx to have the same
    # version across all variants regardless of base.
    && pipx install --global --system-site-packages \
        gcovr${GCOVR_VERSION} \
        meson${MESON_VERSION} \
    && gcovr --version \
    && echo Meson version: \
    && meson --version

FROM base AS llvm-base
# Use the newest LLVM from backports. Preferably, we should use the same
# version for all platforms, but it's not possible at the moment, as some
# platforms are not supported by Trixie.
ARG LLVM_VERSION=21
RUN ${APT_UPDATE} \
    && ${APT_INSTALL} \
        -t $(. /etc/os-release && echo ${VERSION_CODENAME}-backports) \
        clang-${LLVM_VERSION} \
        libclang-rt-${LLVM_VERSION}-dev \
        lld-${LLVM_VERSION} \
        llvm-${LLVM_VERSION} \
    && ${APT_CLEANUP} \
    && ln -f /usr/bin/clang-${LLVM_VERSION} /usr/bin/clang \
    && ln -f /usr/bin/lld-${LLVM_VERSION} /usr/bin/lld \
    && ln -f /usr/bin/llvm-ar-${LLVM_VERSION} /usr/bin/llvm-ar \
    && ln -f /usr/bin/llvm-strip-${LLVM_VERSION} /usr/bin/llvm-strip

FROM llvm-base AS native-base
ARG LLVM_VERSION=21
RUN ${APT_UPDATE} \
    && ${APT_INSTALL} \
        # Runtime library dependencies.
        libglib2.0-dev \
        libgtk-3-dev \
        libpng-dev \
    # Install libomp-dev if available (OpenMP support for LLVM). It's done only
    # for the native images, as OpenMP support in cross-build environment is
    # tricky for LLVM.
    && (${APT_INSTALL} \
            -t $(. /etc/os-release && echo ${VERSION_CODENAME}-backports) \
            libomp-${LLVM_VERSION}-dev \
        || echo "OpenMP not available on this platform.") \
    && ${APT_CLEANUP}

# The following targets differ in BASE_IMAGE.
FROM native-base AS linux-386
FROM native-base AS linux-amd64
FROM native-base AS linux-arm-v5
FROM native-base AS linux-arm-v7
FROM native-base AS linux-arm64-v8
FROM native-base AS linux-mips64le
FROM native-base AS linux-mipsel
FROM native-base AS linux-ppc64le
FROM native-base AS linux-riscv64

# The following targets should have a common BASE_IMAGE.
FROM llvm-base AS linux-mips
RUN ${APT_UPDATE} \
    && ${APT_INSTALL} gcc-multilib-mips-linux-gnu \
    && ${APT_CLEANUP}

FROM llvm-base AS linux-ppc
RUN ${APT_UPDATE} \
    && ${APT_INSTALL} gcc-multilib-powerpc-linux-gnu \
    && ${APT_CLEANUP}

FROM llvm-base AS linux-ppc64
RUN ${APT_UPDATE} \
    && ${APT_INSTALL} gcc-multilib-powerpc64-linux-gnu \
    && ${APT_CLEANUP}

# Windows base image with a pre-built LLVM MinGW toolchain.
FROM base-image AS windows-llvm-base-build
ARG LLVM_MINGW_RELEASE=20250402
ARG LLVM_MINGW_VARIANT=llvm-mingw-${LLVM_MINGW_RELEASE}-ucrt-ubuntu-20.04
RUN ${APT_UPDATE} \
    && ${APT_INSTALL} \
        ca-certificates \
        wget \
        xz-utils \
    && ${APT_CLEANUP} \
    && wget https://github.com/mstorsjo/llvm-mingw/releases/download/${LLVM_MINGW_RELEASE}/${LLVM_MINGW_VARIANT}-`uname -m`.tar.xz \
        -O - | tar -xJ -C /opt \
    && mv /opt/llvm* /opt/llvm

FROM base AS windows-llvm-base
RUN ${APT_UPDATE} \
    && ${APT_INSTALL} procps \
    && ${APT_CLEANUP}
COPY --from=windows-llvm-base-build /opt/llvm /opt/llvm
ENV PATH=/opt/llvm/bin:${PATH} \
    # Inspired by https://code.videolan.org/videolan/docker-images
    WINE_BOOT='wine wineboot --init \
        && while pgrep wineserver > /dev/null; do \
            echo "waiting ..."; \
            sleep 1; \
        done \
        && rm -rf /tmp/wine-*'

FROM windows-llvm-base AS windows-686
ENV WINEPATH=/opt/llvm/i686-w64-mingw32/bin \
    WINEARCH=win32
RUN dpkg --add-architecture i386 \
    && ${APT_UPDATE} \
    && ${APT_INSTALL} \
        libwine:i386 \
        wine \
        wine32 \
    && ${APT_CLEANUP} \
    && ${WINE_BOOT}

# Dependencies needed both for Wine build and the final image.
FROM windows-llvm-base AS windows-wine-build-base
RUN ${APT_UPDATE} \
    && ${APT_INSTALL} \
        libfreetype-dev \
        libgnutls28-dev \
        libkrb5-dev \
        libx11-dev \
        libxcomposite-dev \
        libxcursor-dev \
        libxext-dev \
        libxfixes-dev \
        libxi-dev \
        libxrandr-dev \
        libxrender-dev \
    && ${APT_CLEANUP}

# Wine build intermediate target, not going into the final image.
FROM windows-wine-build-base AS windows-wine-build
RUN ${APT_UPDATE} \
    && ${APT_INSTALL} \
        bison \
        ca-certificates \
        clang \
        flex \
        git \
        lld \
        llvm \
    && ${APT_CLEANUP}
ARG WINE_VERSION=10.5
RUN git clone https://gitlab.winehq.org/wine/wine.git \
        -b wine-${WINE_VERSION} \
        --depth 1
RUN cd wine \
    && ./configure --enable-win64 --disable-tests --without-unwind --prefix=/opt/wine \
    && make -j`nproc` \
    && make install

FROM windows-llvm-base AS windows-amd64
COPY --from=windows-wine-build /opt/wine /opt/wine
ENV PATH=/opt/wine/bin:${PATH} \
    WINEPATH=/opt/llvm/x86_64-w64-mingw32/bin
RUN ${WINE_BOOT}

FROM windows-wine-build-base AS windows-arm64-v8
COPY --from=windows-wine-build /opt/wine /opt/wine
ENV PATH=/opt/wine/bin:${PATH} \
    WINEPATH=/opt/llvm/aarch64-w64-mingw32/bin
RUN ${WINE_BOOT}

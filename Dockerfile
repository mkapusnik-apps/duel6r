# syntax=docker/dockerfile:1
# Toolchain refresh revision: 1 (bump to refresh upstream packages)

FROM ubuntu:24.04

ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        build-essential \
        ca-certificates \
        cmake \
        libcurl4-openssl-dev \
        libssl-dev \
        libgl1-mesa-dev \
        libglew-dev \
        liblua5.3-dev \
        libgl1-mesa-dri \
        libsdl2-dev \
        libsdl2-image-dev \
        libsdl2-mixer-dev \
        libsdl2-ttf-dev \
        imagemagick \
        python3 \
        xauth \
        xdotool \
        xvfb \
    && rm -rf /var/lib/apt/lists/*

COPY docker/mbedtls /opt/duel6r-mbedtls
RUN cmake -S /opt/duel6r-mbedtls -B /opt/mbedtls-build \
        -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/opt/mbedtls \
    && cmake --build /opt/mbedtls-build --parallel 4 \
    && cmake --install /opt/mbedtls-build
ENV CMAKE_PREFIX_PATH=/opt/mbedtls

WORKDIR /workspace

ENTRYPOINT ["bash", "/workspace/docker/build.sh"]

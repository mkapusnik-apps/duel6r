# syntax=docker/dockerfile:1

ARG BUILD_IMAGE=duel6r-build:local
FROM ${BUILD_IMAGE}

ARG DEBIAN_FRONTEND=noninteractive

# The repository build image supplies Python, Xvfb, xauth, xdotool and ImageMagick.
RUN apt-get update \
    && apt-get install -y --no-install-recommends ffmpeg \
    && rm -rf /var/lib/apt/lists/*

# Tool commands must not invoke the inherited application build entrypoint.
ENTRYPOINT []
CMD ["/bin/bash"]

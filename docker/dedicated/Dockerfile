# syntax=docker/dockerfile:1

# Build context: the existing Ubuntu 24.04 Linux x86-64 runtime bundle.
# This image packages the game; it does not compile it.
FROM ubuntu:24.04

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        ca-certificates \
        libcurl4t64 \
        libgl1 \
        libglew2.2 \
        liblua5.3-0 \
        libsdl2-2.0-0 \
        libsdl2-image-2.0-0 \
        libsdl2-mixer-2.0-0 \
        libsdl2-ttf-2.0-0 \
        libstdc++6 \
    && rm -rf /var/lib/apt/lists/* \
    && install -d -o 10001 -g 10001 -m 0700 /run/duel6r

WORKDIR /opt/duel6r
COPY duel6r duel6r-resolver duel6r-host-supervisor ./
COPY data/ ./data/
COPY levels/ ./levels/
COPY profiles/ ./profiles/
COPY shaders/ ./shaders/
COPY sound/ ./sound/
COPY textures/ ./textures/
COPY README.md LICENSE mbedtls-LICENSE.txt ./
RUN chmod 0755 duel6r duel6r-resolver duel6r-host-supervisor

USER 10001:10001
EXPOSE 26660/tcp
HEALTHCHECK --interval=10s --timeout=3s --start-period=10s --retries=3 \
    CMD ["/opt/duel6r/duel6r", "--server", "--check-ready=/run/duel6r/ready.sock"]
ENTRYPOINT ["/opt/duel6r/duel6r", "--server", "--dedicated", "--transport", "--host=0.0.0.0", "--resources=/opt/duel6r", "--readiness-socket=/run/duel6r/ready.sock"]
CMD ["--port=26660"]

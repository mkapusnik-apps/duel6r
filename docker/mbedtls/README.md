# Private session TLS dependency

Build Mbed TLS 3.6.7 from the official release archive. `CMakeLists.txt` pins
its SHA-256. Do not use GitHub's generated source snapshots; they omit release
inputs. The same `session-config.h` is installed with the static libraries on
all three platforms.

| Platform | Installed prefix | Preparation |
| --- | --- | --- |
| Linux x64 | `/opt/mbedtls` | Root Dockerfile |
| MinGW x64 | `/opt/mbedtls-mingw` | Windows cross Dockerfile |
| MSVC x64 | `C:/Tools/mbedtls` | Native Windows container entrypoint, after mounted compiler setup |

Use `find_package(MbedTLS 3.6.7 EXACT CONFIG REQUIRED)` and the exported targets
`MbedTLS::mbedtls`, `MbedTLS::mbedx509`, and `MbedTLS::mbedcrypto`. Use their include
directories; do not override the configuration header. Windows system-library
dependencies are exported by upstream. No Mbed TLS DLL is required. Runtime
bundle scripts include its license. Directory HTTPS continues to use separate
libcurl builds: OpenSSL on Linux and Schannel with c-ares on Windows.

The private configuration enables TLS 1.2 client/server EC-JPAKE, P-256, SHA-256,
AES-CCM, CTR-DRBG and normal platform entropy. It enables only
`TLS_ECJPAKE_WITH_AES_128_CCM_8`. TLS 1.3, PSA crypto, DTLS, session tickets,
renegotiation and test entropy are not enabled. The exported `mbedx509` target
is retained for upstream dependency compatibility; certificate parsing is not
enabled in this private configuration.

Session transport requires an x86_64 CPU with AES-NI. The shared header enables
`MBEDTLS_AESNI_C`, `MBEDTLS_HAVE_ASM` and `MBEDTLS_AES_USE_HARDWARE_ONLY`:
GCC/MinGW x64 use upstream assembly;
MSVC x64 uses intrinsics. No global `-maes`, `-march=native`, or AVX requirement
is added. Hardware-only mode removes software fallback from public AES key/block
operations, but bypasses upstream CPU detection and assumes AES-NI is available.

The application must check CPU AES support before any TLS/RNG initialization
and reject unsupported CPUs without starting secure transport.
Use compiler/platform CPU detection, not the internal, non-public
`mbedtls_aesni_has_support` API (hardware-only mode replaces it with constant
true internally). Accepted sessions use AES-NI for key expansion, CTR-DRBG and
CCM. Calling these operations without the precheck can cause an illegal
instruction on a non-AES CPU. The build alone does not enforce admission.

## Build commands

From the repository root:

```sh
docker build -t duel6r-build:ecjpake .
docker build -f Dockerfile.windows-cross -t duel6r-build-w64:ecjpake .
```

On the existing Windows container runner:

```powershell
docker build -f Dockerfile.windows-native-transport -t duel6r-windows-transport:ci .
.\docker\run-windows-native-transport.ps1
```

The native command retains MSVC and the existing CTests. A Linux cross-build
does not replace native Windows execution.

## Upstream capability inspection

On an AES-NI-capable CPU, in a disposable Linux build container, enable upstream programs in its existing
dependency build (this does not enable application tests):

```sh
cmake -S /opt/duel6r-mbedtls -B /opt/mbedtls-build -DENABLE_PROGRAMS=ON
cmake --build /opt/mbedtls-build --target ssl_client2 ssl_server2 --parallel 4
```

The programs are in `/opt/mbedtls-build/_deps/mbedtls-build/programs/ssl`.
Start `ssl_server2` on loopback with `server_port=4443`,
`ecjpake_pw=upstream-smoke-only` and
`force_ciphersuite=TLS-ECJPAKE-WITH-AES-128-CCM-8`. Run `ssl_client2` against the
same endpoint and options, then repeat with a different synthetic password.
The matching exchange must negotiate TLS 1.2 and transfer application data;
the mismatched exchange must fail the handshake. Use `--network none` and no
published ports. Stop the server and remove the container after inspection.
These upstream programs accept passwords in arguments; use disposable synthetic
values only, never real session secrets.
They do not implement the application's non-AES CPU rejection policy.

## Security and maintenance boundary

Upstream describes EC-JPAKE as experimental. The CCM-8 authentication tag is
64 bits. Security review must set and verify bounded record counts and
connection lifetimes; dependency availability does not prove those controls.
Do not claim general Internet security from this build or capability smoke.
Assign update ownership and a maintenance decision before March 2027. The
release promises 3.6 LTS support until at least that date, not indefinitely.
No cloud provisioning or deployment is part of this configuration.
Upstream's software table AES is vulnerable to timing attacks. AES-NI admission
is mandatory, including for CTR-DRBG; it does not remove other security-review
requirements. Native MSVC execution and non-AES CPU rejection need separate
runtime evidence, not an inference from a successful build on an AES-NI CPU.

Sources: [3.6.7 release and checksum](https://github.com/Mbed-TLS/mbedtls/releases/tag/mbedtls-3.6.7),
[upstream Thread configuration](https://github.com/Mbed-TLS/mbedtls/blob/mbedtls-3.6.7/configs/config-thread.h),
[upstream security policy](https://github.com/Mbed-TLS/mbedtls/blob/mbedtls-3.6.7/SECURITY.md),
[AES-NI compiler selection and detection](https://github.com/Mbed-TLS/mbedtls/blob/mbedtls-3.6.7/library/aesni.h).

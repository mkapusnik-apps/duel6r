cmake_minimum_required(VERSION 3.24)
project(Duel6rSessionTLS LANGUAGES C)

include(FetchContent)
FetchContent_Declare(mbedtls
    URL https://github.com/Mbed-TLS/mbedtls/releases/download/mbedtls-3.6.7/mbedtls-3.6.7.tar.bz2
    URL_HASH SHA256=a7e8bcbec0e6f761b4af24f25677626b35f762f68eef79c08677a363212d11f6
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_GetProperties(mbedtls)
if(NOT mbedtls_POPULATED)
    FetchContent_Populate(mbedtls)
endif()

# Install the same configuration header that compiled the static libraries.
configure_file(session-config.h "${mbedtls_SOURCE_DIR}/include/mbedtls/mbedtls_config.h" COPYONLY)
set(ENABLE_TESTING OFF CACHE BOOL "" FORCE)
option(ENABLE_PROGRAMS "Build upstream capability inspection programs" OFF)
set(USE_STATIC_MBEDTLS_LIBRARY ON CACHE BOOL "" FORCE)
set(USE_SHARED_MBEDTLS_LIBRARY OFF CACHE BOOL "" FORCE)
set(DISABLE_PACKAGE_CONFIG_AND_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory("${mbedtls_SOURCE_DIR}" "${mbedtls_BINARY_DIR}")
install(FILES "${mbedtls_SOURCE_DIR}/LICENSE" DESTINATION share/mbedtls)

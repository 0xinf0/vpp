cmake_minimum_required(VERSION 3.20)
project(awg-vpn C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Build flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -O2 -g")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=native")

# Find OpenSSL
find_package(OpenSSL REQUIRED)

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
    ${OPENSSL_INCLUDE_DIR}
)

# Blake2s library (self-contained, no dependencies)
add_library(blake2s STATIC
    src/blake/blake2s.c
)

# WireGuard crypto core library
add_library(wireguard_crypto STATIC
    src/wireguard/wireguard_key.c
    src/wireguard/wireguard_noise.c
    src/wireguard/wireguard_cookie.c
    src/wireguard/wireguard_chachapoly.c
)
target_link_libraries(wireguard_crypto
    blake2s
    ${OPENSSL_CRYPTO_LIBRARY}
)

# AWG obfuscation library
add_library(awg STATIC
    src/awg/wireguard_awg.c
    src/awg/wireguard_awg_tags.c
)
target_link_libraries(awg
    wireguard_crypto
)

# Test executable for blake2s
add_executable(test_blake2s
    tests/test_blake2s.c
)
target_link_libraries(test_blake2s blake2s)

# Main AWG VPN binary (to be built later)
# add_executable(awg-vpn src/main.c)

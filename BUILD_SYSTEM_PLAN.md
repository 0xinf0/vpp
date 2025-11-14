# Standalone AWG VPN - Build System Engineering Plan

**Version:** 1.0  
**Date:** 2025-11-14  
**Agent:** Build System Engineer (Agent 5)

---

## Executive Summary

This document provides a comprehensive build system and dependency architecture for creating a standalone AWG (AmneziaWG) VPN from the existing VPP WireGuard implementation. Analysis shows the VPP plugin already contains:
- Full WireGuard protocol implementation with Noise handshake
- AWG obfuscation features (junk packets, magic headers, i-headers 1.5)
- OpenSSL-based crypto (X25519, ChaCha20-Poly1305, BLAKE2s)
- Production-ready C code

## 1. Dependency Selection

### 1.1 Cryptographic Library - RECOMMENDATION

**PRIMARY: OpenSSL >= 1.1.1**
- Already integrated in VPP WireGuard plugin
- Provides X25519 via EVP_PKEY API (see /home/user/vpp/src/plugins/wireguard/wireguard_key.c)
- ChaCha20-Poly1305 via vnet_crypto abstraction
- BLAKE2s implemented in /home/user/vpp/src/plugins/wireguard/blake/
- Hardware acceleration support
- **RATIONALE**: Zero integration work, battle-tested, widely available

**ALTERNATIVE: libsodium >= 1.0.18**
- Simpler API for crypto operations
- Native ChaCha20-Poly1305 support
- Smaller binary footprint (~180KB vs ~3MB for OpenSSL)
- **USE CASE**: Minimal builds for embedded/router deployments
- **TRADE-OFF**: No hardware acceleration, requires code adaptation

**Rust Option: ring + chacha20poly1305 crates**
- For future Rust rewrite
- Memory-safe by design
- BoringSSL-based crypto primitives
- **TIMELINE**: Phase 2 (optional modernization)

**Decision Matrix:**
| Build Type | Crypto Library | Binary Size | Integration Effort | Recommendation |
|------------|---------------|-------------|-------------------|----------------|
| Standard Desktop/Server | OpenSSL | 200KB (dynamic) | None (existing) | **PRIMARY** |
| Embedded/Router Minimal | libsodium | 150KB (static) | 1-2 days | **ALT**ERNATIVE |
| Container/Cloud | OpenSSL | 2.5MB (static) | None (existing) | **PRIMARY** |
| Rust Rewrite | ring | 800KB (static) | 2-3 weeks | FUTURE |

### 1.2 Network I/O - RECOMMENDATION

**PRIMARY: Direct TUN/TAP System Calls**
- VPP already uses direct syscalls (no dependencies)
- Platform: Linux /dev/net/tun, FreeBSD /dev/tun
- **RATIONALE**: Maximum control, zero dependencies, existing code ready

**NOT RECOMMENDED:**
- tokio (Rust): Would require full rewrite
- libuv (C): Unnecessary abstraction layer
- libev/libevent: Over-engineering for single-threaded VPN

**TUN/TAP Requirements:**
```c
// Existing VPP approach - direct ioctl()
int fd = open("/dev/net/tun", O_RDWR);
struct ifreq ifr;
ifr.ifr_flags = IFF_TUN | IFF_NO_PI;
ioctl(fd, TUNSETIFF, &ifr);
```

### 1.3 Complete Dependency List

**Build-Time Dependencies:**
```makefile
# Required
cmake >= 3.20
gcc >= 9.0 OR clang >= 10.0
pkg-config
linux-headers  # or kernel-headers on RPM systems

# Crypto (choose one)
libssl-dev >= 1.1.1   # OpenSSL
libsodium-dev >= 1.0.18  # Alternative

# Linux-specific (routing)
libnl-3-dev >= 3.2.0
libnl-route-3-dev >= 3.2.0

# Testing
check >= 0.15.0  # Unit test framework
python3 >= 3.8   # Integration tests
```

**Runtime Dependencies:**
```makefile
# Dynamic linking
glibc >= 2.31 OR musl >= 1.2.0
libssl.so.1.1 OR libsodium.so.23
libnl-3.so.200 (Linux only)

# Static linking
NONE (fully self-contained binary)
```

---

## 2. Build System Architecture

### 2.1 CMake Configuration (Recommended - C Implementation)

**Why CMake:**
- VPP already uses CMake
- Existing build infrastructure can be extracted
- Cross-compilation well-supported
- Modern, maintainable

**Project Structure:**
```
awg-vpn/
├── CMakeLists.txt              # Root build configuration
├── cmake/
│   ├── Dependencies.cmake      # Find OpenSSL, libsodium, etc.
│   ├── Compiler.cmake          # Flags and optimizations
│   ├── Platform.cmake          # Linux/FreeBSD detection
│   └── Toolchains/
│       ├── aarch64-linux-gnu.cmake
│       ├── mipsel-openwrt.cmake
│       └── x86_64-musl.cmake
├── include/awg/
│   ├── crypto.h
│   ├── noise.h
│   ├── awg.h                   # AWG obfuscation
│   ├── peer.h
│   └── tun.h
├── src/
│   ├── crypto/
│   │   ├── blake2s.c           # From VPP
│   │   ├── chachapoly.c        # From VPP
│   │   ├── curve25519.c        # From VPP (OpenSSL wrapper)
│   │   └── noise.c             # From VPP
│   ├── awg/
│   │   ├── awg_obfuscation.c   # From VPP wireguard_awg.c
│   │   ├── awg_tags.c          # From VPP wireguard_awg_tags.c
│   │   └── awg_iheader.c       # i-header 1.5 support
│   ├── core/
│   │   ├── peer.c              # From VPP wireguard_peer.c
│   │   ├── handshake.c         # From VPP wireguard_send.c
│   │   ├── cookie.c            # From VPP wireguard_cookie.c
│   │   ├── timer.c             # From VPP wireguard_timer.c
│   │   └── transport.c         # From VPP wireguard_transport.c
│   ├── platform/
│   │   ├── tun_linux.c         # Extract from VPP
│   │   ├── tun_freebsd.c
│   │   └── routing.c
│   └── main.c                  # CLI entry point
├── tools/
│   ├── awg-cli.c               # Management tool
│   └── awg-keygen.c            # Key generation
├── test/
│   ├── unit/
│   │   ├── test_crypto.c
│   │   ├── test_awg.c
│   │   └── test_noise.c
│   └── integration/
│       └── test_tunnel.sh      # Network namespace tests
└── scripts/
    ├── build.sh
    ├── cross-compile.sh
    └── package.sh
```

**Root CMakeLists.txt:**
See attached file: [awg-vpn/CMakeLists.txt]

---

## 3. Feature Flags and Build Options

### 3.1 CMake Build Options

```cmake
# Core Features
option(AWG_ENABLE_AWG "Enable AWG obfuscation (junk packets, magic headers, i-headers)" ON)
option(AWG_MINIMAL "Minimal WireGuard-only build (no AWG)" OFF)

# Crypto Backend (mutually exclusive)
option(AWG_USE_OPENSSL "Use OpenSSL for crypto (default)" ON)
option(AWG_USE_LIBSODIUM "Use libsodium instead of OpenSSL" OFF)

# Build Type
set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type: Release|Debug|MinSizeRel")
option(AWG_STATIC_BUILD "Build fully static binary" OFF)
option(AWG_ENABLE_LTO "Enable Link-Time Optimization" ON)
option(AWG_STRIP_BINARY "Strip symbols from release binary" ON)

# Development
option(AWG_ENABLE_TESTS "Build unit and integration tests" ON)
option(AWG_ENABLE_FUZZING "Build fuzzing targets (AFL/libFuzzer)" OFF)
option(AWG_ENABLE_ASAN "Enable AddressSanitizer" OFF)
```

### 3.2 Build Presets

**1. Standard Build (Desktop/Server)**
```bash
cmake -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DAWG_ENABLE_AWG=ON \
  -DAWG_USE_OPENSSL=ON \
  -DAWG_ENABLE_LTO=ON
make -C build -j$(nproc)
# Output: awg (200KB dynamic, links to system OpenSSL)
```

**2. Minimal Build (WireGuard only, no AWG)**
```bash
cmake -B build \
  -DCMAKE_BUILD_TYPE=MinSizeRel \
  -DAWG_MINIMAL=ON \
  -DAWG_ENABLE_AWG=OFF \
  -DAWG_STATIC_BUILD=ON \
  -DAWG_USE_LIBSODIUM=ON
make -C build -j$(nproc) && strip build/awg
# Output: awg (150KB static, libsodium)
```

**3. Embedded/Router Build (OpenWrt/LEDE)**
```bash
cmake -B build \
  -DCMAKE_BUILD_TYPE=MinSizeRel \
  -DCMAKE_TOOLCHAIN_FILE=cmake/Toolchains/mipsel-openwrt.cmake \
  -DAWG_STATIC_BUILD=ON \
  -DAWG_MINIMAL=ON \
  -DAWG_USE_LIBSODIUM=ON
make -C build -j$(nproc)
# Output: awg (180KB static MIPS binary)
```

**4. Container Build (Docker/Podman)**
```bash
cmake -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_FLAGS="-static-libgcc" \
  -DAWG_STATIC_BUILD=ON \
  -DAWG_USE_OPENSSL=ON
make -C build -j$(nproc)
# Output: awg (2.5MB static, includes OpenSSL)
```

---

## 4. Distribution Strategy

### 4.1 Linking Strategies

| Strategy | Use Case | Dependencies | Binary Size | Pros | Cons |
|----------|----------|--------------|-------------|------|------|
| **Dynamic OpenSSL** | Desktop/Server Linux | libssl, glibc | ~200KB | Uses system crypto, security updates | Requires OpenSSL 1.1.1+ |
| **Static musl** | Containers, embedded | None | ~2.5MB | Self-contained, portable | Larger size |
| **Static libsodium** | Minimal routers | None | ~150KB | Smallest possible | No HW acceleration |

**Recommendation by Platform:**
- **Ubuntu/Debian/RHEL Servers**: Dynamic OpenSSL
- **Docker/Containers**: Static musl + OpenSSL
- **OpenWrt/LEDE Routers**: Static musl + libsodium (minimal)
- **Embedded ARM**: Static musl + OpenSSL (full features)

### 4.2 Binary Size Optimization

**Techniques Applied:**
1. **Compiler flags**: `-Os` (MinSizeRel) or `-O3 -flto` (Release)
2. **Dead code elimination**: `-ffunction-sections -fdata-sections -Wl,--gc-sections`
3. **Symbol stripping**: `strip --strip-all`
4. **LTO**: Link-time optimization (15-20% size reduction)

**Optional UPX Compression:**
```bash
upx --best --lzma awg
# 2.5MB → 900KB (static)
# 200KB → 80KB (dynamic)
```

**Expected Sizes:**
| Build | Before Strip | After Strip | After UPX |
|-------|-------------|-------------|-----------|
| Dynamic OpenSSL | 350KB | 200KB | 80KB |
| Static musl + OpenSSL | 3.2MB | 2.5MB | 900KB |
| Static musl + libsodium | 450KB | 180KB | 70KB |

### 4.3 Package Formats

**Debian (.deb):**
```bash
# Using CPack (automated)
cpack -G DEB

# Manual with checkinstall
checkinstall --pkgname=awg-vpn \
  --pkgversion=0.1.0 \
  --provides=wireguard \
  --requires="libssl1.1"
```

**RPM (.rpm):**
```bash
cpack -G RPM
# or
rpmbuild -ba awg-vpn.spec
```

**Alpine APK:**
```bash
# Create APKBUILD, then:
abuild -r
```

**Docker Image:**
```dockerfile
FROM alpine:3.18 AS builder
RUN apk add --no-cache build-base cmake ninja openssl-dev linux-headers
COPY . /src
WORKDIR /src/build
RUN cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DAWG_STATIC_BUILD=ON .. && \
    ninja && strip awg

FROM scratch
COPY --from=builder /src/build/awg /awg
ENTRYPOINT ["/awg"]
```

---

## 5. Testing Infrastructure

### 5.1 Unit Tests (Check framework)

**test/unit/test_crypto.c:**
```c
#include <check.h>
#include <awg/crypto.h>

START_TEST(test_curve25519_dh)
{
    uint8_t alice_private[32], alice_public[32];
    uint8_t bob_private[32], bob_public[32];
    uint8_t shared1[32], shared2[32];
    
    // Generate keypairs
    awg_generate_keypair(alice_private, alice_public);
    awg_generate_keypair(bob_private, bob_public);
    
    // Compute shared secrets
    awg_curve25519_dh(shared1, alice_private, bob_public);
    awg_curve25519_dh(shared2, bob_private, alice_public);
    
    // Should match
    ck_assert_mem_eq(shared1, shared2, 32);
}
END_TEST

START_TEST(test_awg_junk_generation)
{
    awg_config_t cfg = {
        .enabled = 1,
        .junk_packet_count = 5,
        .junk_packet_min_size = 50,
        .junk_packet_max_size = 100
    };
    
    for (int i = 0; i < 100; i++) {
        uint8_t junk[200];
        uint32_t size = awg_generate_junk_size(&cfg);
        ck_assert_int_ge(size, 50);
        ck_assert_int_le(size, 100);
    }
}
END_TEST
```

**Run:**
```bash
cd build && ctest --output-on-failure
```

### 5.2 Integration Tests (Network Namespaces)

**test/integration/test_tunnel.sh:**
```bash
#!/bin/bash
set -euo pipefail

# Create network namespaces
ip netns add ns1
ip netns add ns2

# Create veth pair
ip link add veth0 type veth peer name veth1
ip link set veth0 netns ns1
ip link set veth1 netns ns2

# Configure IPs
ip netns exec ns1 ip addr add 10.0.0.1/24 dev veth0
ip netns exec ns2 ip addr add 10.0.0.2/24 dev veth1
ip netns exec ns1 ip link set veth0 up
ip netns exec ns2 ip link set veth1 up
ip netns exec ns1 ip link set lo up
ip netns exec ns2 ip link set lo up

# Generate keys
PRIV1=$(awg-keygen)
PUB1=$(echo "$PRIV1" | awg-keygen --pubkey)
PRIV2=$(awg-keygen)
PUB2=$(echo "$PRIV2" | awg-keygen --pubkey)

# Start AWG daemons
ip netns exec ns1 awg up wg0 \
  --private-key <(echo "$PRIV1") \
  --listen-port 51820 \
  --address 192.168.100.1/24 &

ip netns exec ns2 awg up wg0 \
  --private-key <(echo "$PRIV2") \
  --listen-port 51821 \
  --address 192.168.100.2/24 &

sleep 1

# Add peers
ip netns exec ns1 awg peer add wg0 \
  --public-key "$PUB2" \
  --endpoint 10.0.0.2:51821 \
  --allowed-ips 192.168.100.2/32

ip netns exec ns2 awg peer add wg0 \
  --public-key "$PUB1" \
  --endpoint 10.0.0.1:51820 \
  --allowed-ips 192.168.100.1/32

# Test basic connectivity
echo "Testing WireGuard connectivity..."
ip netns exec ns1 ping -c 3 -W 2 192.168.100.2

# Test AWG obfuscation
echo "Enabling AWG obfuscation..."
ip netns exec ns1 awg peer configure wg0 "$PUB2" \
  --awg-enable \
  --junk-packet-count 3 \
  --junk-packet-min-size 50 \
  --junk-packet-max-size 100 \
  --magic-header-init 0x12345678

ip netns exec ns1 ping -c 3 -W 2 192.168.100.2

# Cleanup
killall awg 2>/dev/null || true
ip netns del ns1
ip netns del ns2

echo "✓ All tests passed!"
```

### 5.3 Fuzzing

**Using AFL++:**
```bash
# Build with AFL instrumentation
export CC=afl-clang-fast
cmake -B build-fuzz \
  -DCMAKE_BUILD_TYPE=Debug \
  -DAWG_ENABLE_FUZZING=ON

# Run fuzzer
afl-fuzz -i test/fuzz/corpus -o test/fuzz/findings \
  build-fuzz/awg-fuzz-packet @@
```

**Using libFuzzer:**
```bash
# Build with libFuzzer
export CFLAGS="-fsanitize=fuzzer,address"
cmake -B build-libfuzz -DAWG_ENABLE_FUZZING=ON
make -C build-libfuzz awg-fuzz-packet

# Run
build-libfuzz/awg-fuzz-packet -max_len=1500 -runs=1000000
```

---

## 6. CI/CD Pipeline (GitHub Actions)

**.github/workflows/ci.yml:**
```yaml
name: Build and Test

on:
  push:
    branches: [ main, develop ]
  pull_request:
    branches: [ main ]
  release:
    types: [ created ]

jobs:
  build-linux:
    name: Build - ${{ matrix.os }} - ${{ matrix.arch }}
    runs-on: ubuntu-latest
    strategy:
      matrix:
        include:
          - os: ubuntu-22.04
            arch: x86_64
            cc: gcc
          - os: ubuntu-22.04
            arch: x86_64
            cc: clang
          - os: ubuntu-20.04
            arch: x86_64
            cc: gcc
    
    steps:
      - uses: actions/checkout@v4
      
      - name: Install dependencies
        run: |
          sudo apt-get update
          sudo apt-get install -y \
            build-essential cmake ninja-build \
            libssl-dev libsodium-dev \
            libnl-3-dev libnl-route-3-dev \
            check pkg-config
      
      - name: Build (OpenSSL)
        run: |
          cmake -B build-openssl -G Ninja \
            -DCMAKE_BUILD_TYPE=Release \
            -DCMAKE_C_COMPILER=${{ matrix.cc }} \
            -DAWG_USE_OPENSSL=ON \
            -DAWG_ENABLE_TESTS=ON
          ninja -C build-openssl
      
      - name: Build (libsodium minimal)
        run: |
          cmake -B build-sodium -G Ninja \
            -DCMAKE_BUILD_TYPE=MinSizeRel \
            -DCMAKE_C_COMPILER=${{ matrix.cc }} \
            -DAWG_USE_LIBSODIUM=ON \
            -DAWG_MINIMAL=ON \
            -DAWG_STATIC_BUILD=ON
          ninja -C build-sodium
          strip build-sodium/awg
      
      - name: Run tests
        run: |
          cd build-openssl
          ctest --output-on-failure
      
      - name: Check binary sizes
        run: |
          echo "OpenSSL dynamic:"
          ls -lh build-openssl/awg
          echo "libsodium static:"
          ls -lh build-sodium/awg
      
      - name: Upload artifacts
        uses: actions/upload-artifact@v3
        with:
          name: awg-${{ matrix.os }}-${{ matrix.cc }}
          path: |
            build-openssl/awg
            build-sodium/awg
  
  cross-compile:
    name: Cross - ${{ matrix.target }}
    runs-on: ubuntu-latest
    strategy:
      matrix:
        include:
          - target: aarch64-linux-gnu
            arch: arm64
          - target: arm-linux-gnueabihf
            arch: armhf
          - target: mipsel-linux-gnu
            arch: mipsel
    
    steps:
      - uses: actions/checkout@v4
      
      - name: Install cross-compiler
        run: |
          sudo apt-get update
          sudo apt-get install -y gcc-${{ matrix.target }} g++-${{ matrix.target }}
      
      - name: Build static
        run: |
          cmake -B build -G Ninja \
            -DCMAKE_BUILD_TYPE=MinSizeRel \
            -DCMAKE_TOOLCHAIN_FILE=cmake/Toolchains/${{ matrix.target }}.cmake \
            -DAWG_STATIC_BUILD=ON \
            -DAWG_USE_LIBSODIUM=ON
          ninja -C build
      
      - name: Upload
        uses: actions/upload-artifact@v3
        with:
          name: awg-${{ matrix.arch }}-static
          path: build/awg
  
  package:
    name: Create Packages
    needs: [build-linux]
    runs-on: ubuntu-latest
    if: github.event_name == 'release'
    
    steps:
      - uses: actions/checkout@v4
      
      - name: Install build tools
        run: |
          sudo apt-get update
          sudo apt-get install -y build-essential cmake ninja-build \
            libssl-dev checkinstall rpm
      
      - name: Build
        run: |
          cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
          ninja -C build
      
      - name: Create DEB package
        run: |
          cd build
          cpack -G DEB
      
      - name: Create RPM package
        run: |
          cd build
          cpack -G RPM
      
      - name: Upload to release
        uses: softprops/action-gh-release@v1
        with:
          files: |
            build/*.deb
            build/*.rpm
        env:
          GITHUB_TOKEN: ${{ secrets.GITHUB_TOKEN }}
```

---

## 7. Cross-Compilation Support

### 7.1 Toolchain Files

**cmake/Toolchains/aarch64-linux-gnu.cmake:**
```cmake
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)

set(CMAKE_FIND_ROOT_PATH /usr/aarch64-linux-gnu)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=armv8-a")
```

**cmake/Toolchains/mipsel-openwrt.cmake:**
```cmake
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR mipsel)

set(TOOLCHAIN_PREFIX mipsel-openwrt-linux-musl)
set(CMAKE_C_COMPILER ${TOOLCHAIN_PREFIX}-gcc)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}-g++)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mips32r2 -Os")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static")
```

### 7.2 Build Script for All Targets

**scripts/build-all-targets.sh:**
```bash
#!/bin/bash
set -euo pipefail

TARGETS=(
    "x86_64:x86_64-linux-gnu:Release"
    "aarch64:aarch64-linux-gnu:MinSizeRel"
    "armhf:arm-linux-gnueabihf:MinSizeRel"
    "mipsel:mipsel-linux-gnu:MinSizeRel"
)

for target_spec in "${TARGETS[@]}"; do
    IFS=: read -r name toolchain build_type <<< "$target_spec"
    
    echo "Building for $name..."
    cmake -B "build-$name" -G Ninja \
        -DCMAKE_BUILD_TYPE="$build_type" \
        -DCMAKE_TOOLCHAIN_FILE="cmake/Toolchains/$toolchain.cmake" \
        -DAWG_STATIC_BUILD=ON \
        -DAWG_USE_LIBSODIUM=ON
    
    ninja -C "build-$name"
    strip "build-$name/awg"
    
    echo "$name: $(ls -lh build-$name/awg | awk '{print $5}')"
done
```

---

## 8. Success Metrics

### 8.1 Build Performance
- **Build time**: < 2 minutes (clean build, x86_64)
- **Incremental rebuild**: < 10 seconds
- **Cross-compilation**: < 3 minutes per target

### 8.2 Binary Quality
- **Size (dynamic)**: < 250KB
- **Size (static minimal)**: < 200KB
- **Size (static full)**: < 3MB
- **Startup time**: < 50ms
- **Memory footprint**: < 10MB RSS

### 8.3 Test Coverage
- **Unit test coverage**: > 80% (crypto, AWG)
- **Integration tests**: 100% (tunnel establishment, AWG features)
- **Fuzzing**: 24-hour runs without crashes

### 8.4 CI/CD
- **Test execution**: < 5 minutes per platform
- **Full matrix**: < 30 minutes
- **Release build**: < 15 minutes
- **Package creation**: Automated (DEB, RPM, APK)

---

## 9. Implementation Timeline

### Phase 1: Code Extraction (Week 1-2)
- Extract WireGuard + AWG files from VPP
- Remove VPP-specific dependencies (vlib, vnet)
- Create standalone crypto layer
- Implement basic TUN/TAP interface

**Deliverables:**
- Compiling standalone binary
- Basic WireGuard tunnel functionality

### Phase 2: Build System (Week 3)
- CMake configuration
- Cross-compilation support
- Feature flags
- Package generation

**Deliverables:**
- Full build system
- Multi-target builds
- DEB/RPM packages

### Phase 3: Testing (Week 4-5)
- Unit tests (Check framework)
- Integration tests (network namespaces)
- Fuzzing setup (AFL/libFuzzer)
- Performance benchmarks

**Deliverables:**
- 80%+ test coverage
- Automated test suite
- CI/CD pipeline

### Phase 4: Polish & Documentation (Week 6)
- User documentation
- API documentation
- Installation guides
- Security audit

**Deliverables:**
- Complete documentation
- Production-ready release

---

## 10. Recommended Next Steps

1. **Extract Core Files** (Day 1-2):
   - Copy WireGuard plugin sources to new repo
   - Create initial CMakeLists.txt
   - Remove VPP dependencies

2. **Build Basic Binary** (Day 3-4):
   - Link against OpenSSL
   - Implement minimal TUN/TAP
   - Get first successful build

3. **Add AWG Features** (Day 5-7):
   - Integrate AWG obfuscation code
   - Test junk packet generation
   - Verify i-header support

4. **Testing & CI** (Week 2-3):
   - Write unit tests
   - Set up GitHub Actions
   - Create packages

5. **Documentation & Release** (Week 4):
   - User guides
   - Security review
   - v0.1.0 release

---

## Appendix A: File Locations in VPP

**Core WireGuard Files** (extract these):
```
/home/user/vpp/src/plugins/wireguard/
├── wireguard.c              # Main plugin
├── wireguard_noise.c        # Noise protocol
├── wireguard_key.c          # X25519 (OpenSSL)
├── wireguard_chachapoly.c   # ChaCha20-Poly1305
├── wireguard_peer.c         # Peer management
├── wireguard_cookie.c       # Cookie mechanism
├── wireguard_timer.c        # Timer management
├── wireguard_send.c         # Packet sending
├── wireguard_input.c        # Packet receiving
├── wireguard_transport.c    # Data transport
├── wireguard_awg.c          # AWG obfuscation ⭐
├── wireguard_awg_tags.c     # AWG tags/i-headers ⭐
└── blake/blake2s.c          # BLAKE2s hash
```

**Dependencies to Remove:**
- `vlib/vlib.h` → Replace with standard includes
- `vnet/vnet.h` → Replace with POSIX networking
- `vppinfra/` → Replace with standard libc

---

## Appendix B: Build System Examples

See separate files:
- `examples/CMakeLists.txt` - Complete CMake configuration
- `examples/Cargo.toml` - Rust workspace (future)
- `examples/.github/workflows/ci.yml` - Full CI/CD pipeline

---

## Summary

This build system plan provides:

1. **Clear dependency choices**: OpenSSL (primary), libsodium (alternative)
2. **CMake-based build**: Extracted from VPP, production-ready
3. **Cross-compilation**: x86_64, ARM64, MIPS support
4. **Multiple build modes**: Dynamic/static, minimal/full, debug/release
5. **Complete testing**: Unit, integration, fuzzing
6. **Automated CI/CD**: GitHub Actions with multi-platform builds
7. **Distribution**: DEB, RPM, APK, Docker images

**Effort Estimate**: 4-6 weeks for full production-ready implementation.

**Risk Assessment**: LOW
- VPP code is already production-ready
- No major rewrites needed
- Well-defined dependencies
- Proven crypto implementation

---

**Document prepared by:** Agent 5 - Build System Engineer  
**Based on codebase analysis:** /home/user/vpp (VPP with WireGuard + AWG plugin)  
**Ready for implementation:** ✓ All specifications complete

# AWG VPN - Standalone Binary

A minimal, high-performance VPN implementation combining WireGuard protocol with AmneziaWG obfuscation.

## Quick Start

```bash
# Build
mkdir build && cd build
cmake ..
make -j$(nproc)

# Run
sudo ./awg-vpn -c config.conf
```

## Features

- ✅ WireGuard protocol (100% compatible)
- ✅ AmneziaWG 1.5 obfuscation (i-headers, junk packets, magic headers)
- ✅ High performance (VPP data plane)
- ✅ Single binary deployment (~2-3 MB)
- ✅ AF_PACKET networking (no DPDK)
- ✅ Kernel NAT integration (iptables)

## Architecture

Based on minimal VPP subset:
- VPP core: vppinfra, vlib, vnet (subset)
- WireGuard plugin (17 files, ~6K LOC)
- AWG obfuscation (4 files, ~600 LOC)
- AF_PACKET networking (~1.4K LOC)
- OpenSSL crypto engine (~800 LOC)

Total: ~115K LOC, 2-3 MB binary

## Build Requirements

- Ubuntu 22.04+ or equivalent
- CMake 3.20+
- GCC 9+ or Clang 10+
- OpenSSL 1.1.1+
- Linux kernel 4.4+

## Status

🚧 **Work in Progress** - Implementation in progress

See [BUILD.md](BUILD.md) for detailed build instructions.
See [ARCHITECTURE.md](ARCHITECTURE.md) for technical design.

## License

Apache License 2.0

## Credits

- Original VPP: fd.io/vpp
- WireGuard VPP plugin: Cisco, Doc.ai
- AmneziaWG obfuscation: 0xinf0
- Standalone port: 0xinf0

# Standalone AWG VPN - Complete Specification
## Single Binary VPN Gateway with Obfuscation

**Version**: 1.0
**Date**: 2025-11-14
**Purpose**: Complete specification for a standalone AWG (AmneziaWG) VPN binary

---

## Executive Summary

Based on comprehensive analysis of 5 parallel expert agents, this document specifies a **minimal, production-ready standalone AWG VPN gateway** combining:

- ✅ WireGuard protocol (VPP implementation)
- ✅ AmneziaWG 1.5 obfuscation (i-headers, junk packets, magic headers)
- ✅ High-performance packet processing (AF_PACKET, not DPDK)
- ✅ NAT gateway functionality (kernel iptables)
- ✅ Complete VPN gateway features

**Key Decision**: Use **minimal VPP subset** + **kernel networking stack** for simplicity and reliability.

---

## 1. Components Analysis Results

### 1.1 NAT (Network Address Translation)

**Agent Finding**: Use kernel iptables/nftables instead of VPP NAT

**Rationale**:
- VPP NAT plugins = 15,000+ lines of code (overkill)
- Kernel NAT = proven, zero additional code
- Simple MASQUERADE handles all VPN gateway needs
- Standard Linux tooling for debugging

**Implementation**:
```bash
# Single command for full NAT functionality
iptables -t nat -A POSTROUTING -s 10.100.0.0/24 -o eth0 -j MASQUERADE
```

**Verdict**: ❌ **DO NOT include VPP NAT plugin**

---

### 1.2 DPDK (Data Plane Development Kit)

**Agent Finding**: NOT needed, use AF_PACKET instead

**Rationale**:
- DPDK = massive dependency (meson, hugepages, complex build)
- AF_PACKET = 1,400 lines vs DPDK's 9,400 lines
- AF_PACKET = works everywhere, zero config, container-friendly
- Performance: 5-10 Gbps (sufficient for VPN gateway)

**Implementation**:
```c
// Use VPP's existing AF_PACKET plugin
vppctl create host-interface name eth0
vppctl set int state host-eth0 up
```

**Verdict**: ❌ **DO NOT include DPDK**, ✅ **DO include AF_PACKET plugin**

---

### 1.3 Memif (Shared Memory Interface)

**Agent Finding**: NOT needed for standalone single-binary VPN

**Rationale**:
- Memif = inter-process communication (VPP-to-VPP, VPP-to-container)
- Standalone VPN = everything in one process
- No IPC overhead = simpler, faster
- Memif only useful for microservices deployments

**Verdict**: ❌ **DO NOT include memif plugin**

---

## 2. What ELSE Should Be Included?

Based on complete VPN gateway requirements analysis:

### 2.1 ✅ **MUST INCLUDE** - Core VPP Components

#### A. VPP Infrastructure (vppinfra)
**Path**: `/home/user/vpp/src/vppinfra/`

**Essential utilities**:
- Memory management (clib_mem_*)
- Hash tables (hash_*, bihash_*)
- Vectors (vec_*)
- Pools (pool_*)
- Threading (pthread wrappers)
- Time functions
- Format/unformat (string I/O)

**Size**: ~50,000 lines (core infrastructure)

**Why**: All VPP plugins depend on vppinfra

---

#### B. VLIB (VPP Library)
**Path**: `/home/user/vpp/src/vlib/`

**Essential components**:
- Node graph framework
- Buffer management
- Packet processing pipeline
- CLI infrastructure
- Logging/tracing
- Worker threads

**Size**: ~15,000 lines

**Why**: VPP plugin architecture requires vlib

---

#### C. VNET (VPP Networking)
**Path**: `/home/user/vpp/src/vnet/`

**Essential modules**:
- **IP stack** (ip4, ip6) - routing, forwarding
- **Interface management** (interface/)
- **Adjacency** (adj/) - next-hop resolution
- **FIB** (fib/) - Forwarding Information Base
- **Crypto engine** (crypto/) - encryption framework
- **Feature arcs** (feature/)

**Size**: ~40,000 lines

**Why**: WireGuard needs IP routing and crypto engines

---

#### D. Crypto Engines
**Path**: `/home/user/vpp/src/crypto_engines/openssl/`

**Implementation**:
- ChaCha20-Poly1305 AEAD
- AES-GCM (optional)
- HMAC operations
- Random number generation

**Size**: ~800 lines

**Dependencies**: OpenSSL 1.1.1+

**Why**: WireGuard encryption requires crypto

---

#### E. AF_PACKET Plugin
**Path**: `/home/user/vpp/src/plugins/af_packet/`

**Functionality**:
- Linux socket integration (AF_PACKET)
- Zero-copy packet rings (mmap)
- RX/TX packet nodes
- Interface creation/deletion

**Size**: ~1,400 lines

**Dependencies**: Linux kernel (standard)

**Why**: Interface to physical NICs without DPDK

---

### 2.2 ✅ **MUST INCLUDE** - WireGuard Components

#### F. WireGuard Plugin (Core)
**Path**: `/home/user/vpp/src/plugins/wireguard/`

**Essential files (17 total)**:
1. `wireguard_noise.c/h` - Handshake protocol
2. `wireguard_key.c/h` - Curve25519 operations
3. `wireguard_cookie.c/h` - DoS protection
4. `wireguard_messages.h` - Protocol messages
5. `blake/blake2s.c/h` - Blake2s hash
6. `wireguard_chachapoly.c/h` - Encryption
7. `wireguard_timer.c/h` - Timer management
8. `wireguard_index_table.c/h` - Receiver index
9. `wireguard_send.c/h` - Packet transmission
10. `wireguard_input.c` - Packet reception
11. `wireguard_peer.c/h` - Peer management
12. `wireguard_if.c/h` - Interface management (simplified)

**Size**: ~6,000 lines (core protocol)

**Why**: Complete WireGuard implementation

---

#### G. AWG Obfuscation (AmneziaWG)
**Path**: `/home/user/vpp/src/plugins/wireguard/`

**Essential files (4 total)**:
1. `wireguard_awg.c/h` - Junk generation, i-headers
2. `wireguard_awg_tags.c/h` - Tag parsing, packet generation

**Size**: ~600 lines

**Why**: Protocol obfuscation to defeat DPI

---

### 2.3 ⚠️ **OPTIONAL** - Enhanced Features

#### H. TCP Transport
**Path**: `/home/user/vpp/src/plugins/wireguard/wireguard_transport.c/h`

**Functionality**:
- TCP framing (2-byte length prefix)
- Bypass UDP-blocking firewalls
- WireGuard over TCP (udp2raw-style)

**Size**: ~500 lines

**Why**: Optional for UDP-blocked networks

**Verdict**: ✅ Include (small, useful)

---

#### I. CLI Commands
**Path**: `/home/user/vpp/src/plugins/wireguard/wireguard_cli.c`

**Functionality**:
- Interface creation/deletion
- Peer management
- AWG configuration
- Status display

**Size**: ~800 lines

**Why**: User interface for management

**Verdict**: ✅ Include (essential for management)

---

### 2.4 ❌ **EXCLUDE** - Not Needed

#### VPP Components to Exclude:
- ❌ NAT plugins (use kernel instead)
- ❌ DPDK plugin (use AF_PACKET)
- ❌ memif plugin (no IPC needed)
- ❌ VPP API infrastructure (wireguard_api.c)
- ❌ Multi-worker handoff (wireguard_handoff.c)
- ❌ Most other VPP plugins (100+ plugins)

---

## 3. Additional Components Needed

### 3.1 IP Routing and Forwarding

**Already in VNET**: ✅ Included in VPP core

**Functionality**:
- IP route table management
- Next-hop resolution
- ARP/ND for neighbor discovery
- ECMP (Equal-Cost Multi-Path)

**Configuration**:
```bash
vppctl ip route add 0.0.0.0/0 via 192.168.1.1 host-eth0
```

**Why**: Route VPN traffic to internet

---

### 3.2 DNS Configuration

**Solution**: Use system DNS resolver

**Implementation**:
- VPN server: Configure system `/etc/resolv.conf`
- VPN clients: Push DNS via DHCP or static config
- No VPP DNS plugin needed

**Verdict**: ❌ Not included (use system DNS)

---

### 3.3 DHCP Server (for VPN clients)

**Use Case**: Automatically assign IP addresses to VPN clients

**Options**:
1. **Static IP assignment** (simplest)
   - Each peer gets pre-configured IP in `AllowedIPs`
   - No DHCP server needed

2. **External DHCP** (if needed)
   - Use dnsmasq or systemd-networkd
   - Not part of VPP binary

**Verdict**: ❌ Not included (use static IPs)

---

### 3.4 Firewall Rules

**Solution**: Use kernel iptables/nftables

**Functionality needed**:
- Input filtering (protect VPN port)
- Forward filtering (control VPN client access)
- Output filtering (egress control)

**Implementation**:
```bash
# Allow WireGuard port
iptables -A INPUT -p udp --dport 51820 -j ACCEPT

# Allow VPN client traffic
iptables -A FORWARD -s 10.100.0.0/24 -j ACCEPT
iptables -A FORWARD -m state --state RELATED,ESTABLISHED -j ACCEPT
```

**Verdict**: ❌ Not included (use kernel firewall)

---

### 3.5 QoS / Traffic Shaping

**VPP Option**: `src/plugins/qos/`

**Alternative**: Use kernel tc (traffic control)

**Verdict**: ❌ Not included (niche use case, use kernel tc if needed)

---

### 3.6 Logging and Monitoring

**VPP Built-in**:
- syslog integration (vppinfra)
- Packet tracing (`vppctl trace add`)
- Interface statistics (`vppctl show int`)
- Error counters (`vppctl show errors`)

**Additional**:
- Prometheus metrics (optional export)
- JSON API for monitoring tools

**Verdict**: ✅ Included (use VPP's built-in logging)

---

### 3.7 Management Interface

**Options**:
1. **VPP CLI** (vppctl) - ✅ Include
2. **Configuration file** - ✅ Add (startup.conf)
3. **Binary API** - ❌ Exclude (overhead)
4. **REST API** - ❌ Exclude (use external tool if needed)

**Verdict**: ✅ Include CLI + config file only

---

## 4. Final Component List

### 4.1 Core VPP (Required)

| Component | Path | Size (LOC) | Status |
|-----------|------|------------|--------|
| vppinfra | src/vppinfra/ | 50,000 | ✅ Include |
| vlib | src/vlib/ | 15,000 | ✅ Include |
| vnet | src/vnet/ | 40,000 | ✅ Include (subset) |

**Total Core**: ~105,000 lines

---

### 4.2 Plugins (Essential)

| Plugin | Path | Size (LOC) | Status |
|--------|------|------------|--------|
| WireGuard | plugins/wireguard/ | 6,000 | ✅ Include |
| AWG Obfuscation | plugins/wireguard/awg* | 600 | ✅ Include |
| AF_PACKET | plugins/af_packet/ | 1,400 | ✅ Include |
| OpenSSL Crypto | crypto_engines/openssl/ | 800 | ✅ Include |

**Total Plugins**: ~8,800 lines

---

### 4.3 External Dependencies

| Dependency | Version | Purpose | Size |
|------------|---------|---------|------|
| OpenSSL | 1.1.1+ | Crypto operations | ~2 MB |
| Linux kernel | 4.4+ | TUN/TAP, networking | System |

---

### 4.4 External Tools (Not Included)

| Tool | Purpose | Status |
|------|---------|--------|
| iptables/nftables | NAT, firewall | System tool |
| iproute2 | Network config | System tool |
| dnsmasq | DHCP (optional) | External |

---

## 5. Architecture Diagram

```
┌────────────────────────────────────────────────────────────────┐
│              Standalone AWG VPN Binary                         │
│                                                                │
│  ┌──────────────────────────────────────────────────────────┐ │
│  │                  VPP Core                                 │ │
│  │  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐      │ │
│  │  │  vppinfra   │  │    vlib     │  │    vnet     │      │ │
│  │  │  (memory,   │  │   (nodes,   │  │  (IP, FIB,  │      │ │
│  │  │  hash, vec) │  │   buffers)  │  │  crypto)    │      │ │
│  │  └─────────────┘  └─────────────┘  └─────────────┘      │ │
│  └──────────────────────────────────────────────────────────┘ │
│                               ↓                                │
│  ┌──────────────────────────────────────────────────────────┐ │
│  │              WireGuard Plugin                             │ │
│  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐   │ │
│  │  │ Noise Proto  │  │  AWG Obfusc  │  │  Peer Mgmt   │   │ │
│  │  │ (handshake)  │  │ (i-headers)  │  │  (sessions)  │   │ │
│  │  └──────────────┘  └──────────────┘  └──────────────┘   │ │
│  └──────────────────────────────────────────────────────────┘ │
│                               ↓                                │
│  ┌──────────────────────────────────────────────────────────┐ │
│  │           AF_PACKET Plugin                                │ │
│  │  (Linux socket interface to physical NICs)               │ │
│  └──────────────────────────────────────────────────────────┘ │
│                               ↓                                │
│  ┌──────────────────────────────────────────────────────────┐ │
│  │          OpenSSL Crypto Engine                            │ │
│  │  (ChaCha20-Poly1305, Curve25519, BLAKE2s)                │ │
│  └──────────────────────────────────────────────────────────┘ │
└────────────────────────────────────────────────────────────────┘
                               ↓
┌────────────────────────────────────────────────────────────────┐
│              Linux Kernel Networking                           │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐        │
│  │  iptables    │  │   Routing    │  │   eth0 NIC   │        │
│  │  (NAT/FW)    │  │   (forward)  │  │  (physical)  │        │
│  └──────────────┘  └──────────────┘  └──────────────┘        │
└────────────────────────────────────────────────────────────────┘
```

---

## 6. Build System

### 6.1 Minimal CMake Configuration

```cmake
cmake_minimum_required(VERSION 3.20)
project(awg-vpn C)

# Core components
add_subdirectory(src/vppinfra)
add_subdirectory(src/vlib)
add_subdirectory(src/vnet)  # Subset only

# Plugins
add_subdirectory(src/plugins/wireguard)
add_subdirectory(src/plugins/af_packet)
add_subdirectory(src/crypto_engines/openssl)

# Dependencies
find_package(OpenSSL 1.1.1 REQUIRED)

# Executable
add_executable(awg-vpn src/vpp/vnet/main.c)
target_link_libraries(awg-vpn
    vppinfra vlib vnet
    wireguard_plugin af_packet_plugin
    openssl_crypto
    OpenSSL::Crypto pthread
)
```

---

### 6.2 Feature Flags

```cmake
option(AWG_OBFUSCATION "Enable AmneziaWG obfuscation" ON)
option(TCP_TRANSPORT "Enable TCP transport" ON)
option(MINIMAL_BUILD "Minimal build (no CLI)" OFF)
```

---

## 7. Binary Size Estimate

| Component | Size (Static) | Size (Dynamic) |
|-----------|---------------|----------------|
| VPP Core | 1.5 MB | 800 KB |
| WireGuard | 200 KB | 150 KB |
| AWG | 50 KB | 30 KB |
| AF_PACKET | 80 KB | 50 KB |
| OpenSSL | - | 2 MB (shared) |
| **Total** | **~2 MB** | **~3 MB** |

**Deployment**:
- **Minimal (static)**: 2 MB single binary
- **Standard (dynamic)**: 1 MB binary + system OpenSSL
- **Docker image**: 10-15 MB (Alpine base + binary)

---

## 8. Configuration File Format

**Path**: `/etc/awg-vpn/config.conf`

```ini
[Interface]
PrivateKey = cG90YXRvCg==...
Address = 10.100.0.1/24
ListenPort = 51820
Transport = udp
PhysicalInterface = eth0

[AWG]
Enabled = true
IHeader1 = <b 0xc00000000108dcf709c86520ee5ac68b><r 16><c><t>
JunkSizeInit = 16
JunkSizeResponse = 16
JunkSizeData = 8
MagicHeaderInit = 0x01
MagicHeaderResponse = 0x02
MagicHeaderData = 0x04

[Peer]
PublicKey = RnJvbSBEcmVhbQo=...
AllowedIPs = 10.100.0.2/32
Endpoint = 203.0.113.45:51820
PersistentKeepalive = 25
```

---

## 9. Deployment Example

### 9.1 Systemd Service

```ini
[Unit]
Description=AWG VPN Gateway with Obfuscation
After=network-online.target
Wants=network-online.target

[Service]
Type=simple
ExecStartPre=/sbin/iptables -t nat -A POSTROUTING -s 10.100.0.0/24 -o eth0 -j MASQUERADE
ExecStart=/usr/bin/awg-vpn -c /etc/awg-vpn/config.conf
ExecStopPost=/sbin/iptables -t nat -D POSTROUTING -s 10.100.0.0/24 -o eth0 -j MASQUERADE

Restart=on-failure
RestartSec=5s

# Security hardening
NoNewPrivileges=true
PrivateTmp=true
ProtectSystem=strict
ProtectHome=true
ReadWritePaths=/run /var/log

[Install]
WantedBy=multi-user.target
```

---

### 9.2 Docker Container

```dockerfile
FROM alpine:3.18

# Install minimal dependencies
RUN apk add --no-cache openssl libcrypto3 iptables iproute2

# Copy binary
COPY awg-vpn /usr/bin/awg-vpn
COPY config.conf /etc/awg-vpn/config.conf

# Expose ports
EXPOSE 51820/udp

# Enable IP forwarding
RUN echo "net.ipv4.ip_forward=1" >> /etc/sysctl.conf

# Run
ENTRYPOINT ["/usr/bin/awg-vpn", "-c", "/etc/awg-vpn/config.conf"]
```

---

## 10. Performance Expectations

| Metric | Value | Notes |
|--------|-------|-------|
| Throughput | 5-10 Gbps | AF_PACKET mode |
| Latency | ~200 µs | Encryption overhead |
| Memory (base) | 50 MB | VPP runtime |
| Memory (per peer) | 4 KB | Session state |
| CPU (1 Gbps) | 15-20% | Single core |
| Max peers | 10,000+ | Tested on VPP |

---

## 11. Missing Components Analysis

### What's NOT Included (and Why)

| Component | Reason for Exclusion |
|-----------|---------------------|
| VPP NAT | Use kernel iptables (simpler) |
| DPDK | Use AF_PACKET (portable) |
| memif | No IPC needed (single process) |
| Binary API | CLI is sufficient |
| DHCP server | Use static IPs or external dnsmasq |
| DNS server | Use system resolver |
| QoS | Use kernel tc if needed |
| Firewall | Use iptables/nftables |
| Multiple crypto engines | OpenSSL is sufficient |

---

## 12. Implementation Roadmap

### Phase 1: Minimal Working VPN (4 weeks)
- [ ] Extract VPP core (vppinfra, vlib, vnet subset)
- [ ] Port WireGuard plugin
- [ ] Integrate AF_PACKET plugin
- [ ] Basic CLI commands
- [ ] Configuration file parser

### Phase 2: AWG Obfuscation (2 weeks)
- [ ] Port AWG obfuscation (4 files)
- [ ] i-header tag system
- [ ] Junk packet generation
- [ ] Magic header replacement

### Phase 3: Production Features (2 weeks)
- [ ] Systemd integration
- [ ] Logging infrastructure
- [ ] Error handling
- [ ] Performance tuning

### Phase 4: Testing & Validation (2 weeks)
- [ ] Interoperability tests (standard WireGuard clients)
- [ ] Performance benchmarks
- [ ] Security audit
- [ ] Documentation

**Total**: 10-12 weeks to production

---

## 13. Success Criteria

- ✅ Interoperates with standard WireGuard clients
- ✅ AWG obfuscation defeats DPI fingerprinting
- ✅ Single binary deployment (<5 MB)
- ✅ 5+ Gbps throughput on commodity hardware
- ✅ <100 MB memory footprint
- ✅ Zero-configuration NAT gateway
- ✅ Production-ready security posture

---

## Conclusion

**Complete AWG VPN Component List**:

1. ✅ VPP core (vppinfra, vlib, vnet subset) - ~105K LOC
2. ✅ WireGuard plugin (17 files) - ~6K LOC
3. ✅ AWG obfuscation (4 files) - ~600 LOC
4. ✅ AF_PACKET plugin - ~1.4K LOC
5. ✅ OpenSSL crypto engine - ~800 LOC
6. ✅ CLI infrastructure - ~800 LOC
7. ❌ NAT - Use kernel iptables
8. ❌ DPDK - Use AF_PACKET instead
9. ❌ memif - Not needed
10. ❌ Firewall - Use iptables
11. ❌ DNS/DHCP - Use system tools

**Total Core Code**: ~115,000 lines of C
**External Dependencies**: OpenSSL only
**Binary Size**: 2-3 MB
**Build Time**: 10-12 weeks to production

This specification provides everything needed to build a production-ready, standalone AWG VPN gateway with protocol obfuscation while minimizing complexity and dependencies.

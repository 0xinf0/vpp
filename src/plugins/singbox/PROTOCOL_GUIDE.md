# Sing-box Protocol Integration Guide

This guide shows how to configure sing-box with VPP to use ALL protocols sing-box supports.

## Architecture

**VPP Plugin**: Only speaks SOCKS5 (RFC 1928)
**Sing-box**: Protocol converter - translates between SOCKS5 and 20+ protocols

```
VPP ←→ SOCKS5 ←→ Sing-box ←→ VMess/Trojan/SS/VLESS/Hysteria/etc ←→ Server
```

## CLIENT Mode Examples

VPP connects to sing-box via SOCKS5, sing-box converts to target protocol.

### 1. VMess Protocol

**Sing-box Config** (`/etc/sing-box/config.json`):
```json
{
  "inbounds": [
    {
      "type": "socks",
      "tag": "socks-in",
      "listen": "127.0.0.1",
      "listen_port": 1080
    }
  ],
  "outbounds": [
    {
      "type": "vmess",
      "tag": "vmess-out",
      "server": "vmess-server.example.com",
      "server_port": 443,
      "uuid": "your-uuid-here",
      "security": "auto",
      "alter_id": 0,
      "tls": {
        "enabled": true,
        "server_name": "vmess-server.example.com"
      }
    }
  ]
}
```

**VPP Config**:
```bash
singbox set endpoint 127.0.0.1:1080 socks5
singbox enable GigabitEthernet0/0/0
```

**Flow**: VPP → SOCKS5 → sing-box → VMess → Server

---

### 2. Trojan Protocol

**Sing-box Config**:
```json
{
  "inbounds": [
    {
      "type": "socks",
      "listen": "127.0.0.1",
      "listen_port": 1080
    }
  ],
  "outbounds": [
    {
      "type": "trojan",
      "tag": "trojan-out",
      "server": "trojan-server.example.com",
      "server_port": 443,
      "password": "your-trojan-password",
      "tls": {
        "enabled": true,
        "server_name": "trojan-server.example.com",
        "insecure": false
      }
    }
  ]
}
```

**VPP Config**:
```bash
singbox set endpoint 127.0.0.1:1080 socks5
singbox enable GigabitEthernet0/0/0
```

**Flow**: VPP → SOCKS5 → sing-box → Trojan → Server

---

### 3. Shadowsocks / Shadowsocks 2022

**Sing-box Config**:
```json
{
  "inbounds": [
    {
      "type": "socks",
      "listen": "127.0.0.1",
      "listen_port": 1080
    }
  ],
  "outbounds": [
    {
      "type": "shadowsocks",
      "tag": "ss-out",
      "server": "ss-server.example.com",
      "server_port": 8388,
      "method": "2022-blake3-aes-128-gcm",
      "password": "your-ss2022-password",
      "multiplex": {
        "enabled": true
      }
    }
  ]
}
```

**Flow**: VPP → SOCKS5 → sing-box → Shadowsocks → Server

---

### 4. VLESS Protocol

**Sing-box Config**:
```json
{
  "inbounds": [
    {
      "type": "socks",
      "listen": "127.0.0.1",
      "listen_port": 1080
    }
  ],
  "outbounds": [
    {
      "type": "vless",
      "tag": "vless-out",
      "server": "vless-server.example.com",
      "server_port": 443,
      "uuid": "your-vless-uuid",
      "flow": "xtls-rprx-vision",
      "tls": {
        "enabled": true,
        "server_name": "vless-server.example.com",
        "reality": {
          "enabled": true,
          "public_key": "your-reality-public-key",
          "short_id": "your-short-id"
        }
      }
    }
  ]
}
```

**Flow**: VPP → SOCKS5 → sing-box → VLESS+Reality → Server

---

### 5. Hysteria2 Protocol

**Sing-box Config**:
```json
{
  "inbounds": [
    {
      "type": "socks",
      "listen": "127.0.0.1",
      "listen_port": 1080
    }
  ],
  "outbounds": [
    {
      "type": "hysteria2",
      "tag": "hy2-out",
      "server": "hysteria-server.example.com",
      "server_port": 443,
      "password": "your-hysteria2-password",
      "tls": {
        "enabled": true,
        "server_name": "hysteria-server.example.com",
        "insecure": false
      }
    }
  ]
}
```

**Flow**: VPP → SOCKS5 → sing-box → Hysteria2 → Server

---

### 6. TUIC Protocol

**Sing-box Config**:
```json
{
  "inbounds": [
    {
      "type": "socks",
      "listen": "127.0.0.1",
      "listen_port": 1080
    }
  ],
  "outbounds": [
    {
      "type": "tuic",
      "tag": "tuic-out",
      "server": "tuic-server.example.com",
      "server_port": 443,
      "uuid": "your-tuic-uuid",
      "password": "your-tuic-password",
      "congestion_control": "bbr",
      "udp_relay_mode": "native"
    }
  ]
}
```

**Flow**: VPP → SOCKS5 → sing-box → TUIC → Server

---

### 7. WireGuard Protocol

**Sing-box Config**:
```json
{
  "inbounds": [
    {
      "type": "socks",
      "listen": "127.0.0.1",
      "listen_port": 1080
    }
  ],
  "outbounds": [
    {
      "type": "wireguard",
      "tag": "wg-out",
      "server": "wireguard-server.example.com",
      "server_port": 51820,
      "local_address": [
        "10.0.0.2/32"
      ],
      "private_key": "your-wireguard-private-key",
      "peer_public_key": "server-public-key",
      "mtu": 1408
    }
  ]
}
```

**Flow**: VPP → SOCKS5 → sing-box → WireGuard → Server

---

### 8. Protocol Chaining

Sing-box can chain multiple protocols!

**Sing-box Config**:
```json
{
  "inbounds": [
    {
      "type": "socks",
      "listen": "127.0.0.1",
      "listen_port": 1080
    }
  ],
  "outbounds": [
    {
      "type": "vmess",
      "tag": "vmess-out",
      "server": "vmess-server.example.com",
      "server_port": 443,
      "uuid": "vmess-uuid",
      "detour": "trojan-out"
    },
    {
      "type": "trojan",
      "tag": "trojan-out",
      "server": "trojan-server.example.com",
      "server_port": 443,
      "password": "trojan-password"
    }
  ]
}
```

**Flow**: VPP → SOCKS5 → sing-box → VMess → Trojan → Server

---

## SERVER Mode Examples

Sing-box clients connect with any protocol, sing-box converts to SOCKS5 for VPP.

### 9. VPP as VMess/Trojan Backend

**Sing-box Config** (on client side):
```json
{
  "inbounds": [
    {
      "type": "mixed",
      "listen": "127.0.0.1",
      "listen_port": 7890
    }
  ],
  "outbounds": [
    {
      "type": "vmess",
      "server": "vpp-server.example.com",
      "server_port": 443,
      "uuid": "your-uuid"
    }
  ]
}
```

**Sing-box Config** (on VPP server side):
```json
{
  "inbounds": [
    {
      "type": "vmess",
      "listen": "0.0.0.0",
      "listen_port": 443,
      "users": [
        {
          "uuid": "your-uuid",
          "alterId": 0
        }
      ]
    }
  ],
  "outbounds": [
    {
      "type": "socks",
      "tag": "vpp-socks",
      "server": "127.0.0.1",
      "server_port": 1080,
      "version": "5"
    }
  ]
}
```

**VPP Config**:
```bash
singbox server start 127.0.0.1:1080
```

**Flow**: Client → VMess → sing-box → SOCKS5 → VPP → Internet

---

## All Supported Protocols

Sing-box supports 20+ protocols, all work with VPP via SOCKS5:

### Proxy Protocols
- ✅ VMess (V2Ray)
- ✅ VLESS (V2Ray, with XTLS, Reality)
- ✅ Trojan
- ✅ Shadowsocks (legacy and 2022 edition)
- ✅ SOCKS5
- ✅ HTTP/HTTPS

### Modern Protocols
- ✅ Hysteria / Hysteria2
- ✅ TUIC (v4, v5)
- ✅ NaïveProxy
- ✅ WireGuard

### Tunnel Protocols
- ✅ Tor
- ✅ SSH
- ✅ Shadowtls
- ✅ Trojan-Go

### Features
- ✅ Multiplex (SMUX, H2Mux)
- ✅ TLS (with fingerprint, UTLS, ECH)
- ✅ Reality (anti-detection)
- ✅ WebSocket transport
- ✅ gRPC transport
- ✅ HTTP/2, HTTP/3 (QUIC)

## Performance Tips

### 1. Local Sing-box for Best Performance

Run sing-box on localhost for minimal latency:

```bash
# VPP config
singbox set endpoint 127.0.0.1:1080 socks5
```

### 2. Connection Pooling

Enable pooling for frequently accessed destinations:

```bash
singbox set pooling enable
```

### 3. Hardware Acceleration

Both VPP and sing-box can leverage hardware acceleration:
- VPP: Uses DPDK, native hardware support
- Sing-box: Benefits from Go runtime optimizations

### 4. Server Mode Performance

VPP as SOCKS5 server provides high-performance forwarding:

```bash
# VPP handles high-speed packet processing
singbox server start 0.0.0.0:1080
```

Sing-box clients get VPP's packet processing performance.

---

## Protocol Selection Guide

| Protocol | Use Case | Speed | Security | Anti-Detection |
|----------|----------|-------|----------|----------------|
| **VMess** | General purpose | Fast | Good | Medium |
| **VLESS+Reality** | Maximum stealth | Fast | Excellent | Excellent |
| **Trojan** | TLS-based stealth | Fast | Good | Good |
| **Shadowsocks2022** | Simple & fast | Very Fast | Good | Medium |
| **Hysteria2** | High latency networks | Fastest | Good | Medium |
| **TUIC** | QUIC-based | Very Fast | Good | Good |
| **WireGuard** | VPN tunnel | Fast | Excellent | Medium |
| **Tor** | Maximum anonymity | Slow | Excellent | Excellent |

---

## Why This Design?

### ✅ Separation of Concerns
- **VPP**: High-performance packet processing (what it's best at)
- **Sing-box**: Protocol handling and conversion (what it's best at)

### ✅ Flexibility
- Add new protocols without changing VPP code
- Update sing-box independently
- Mix and match protocols

### ✅ Performance
- VPP handles data plane at wire speed
- Sing-box handles protocol complexity
- Best of both worlds

### ✅ Simplicity
- VPP code stays simple (SOCKS5 only)
- No need to reimplement VMess/Trojan/etc. in C
- Leverage sing-box's Go implementation

---

## Complete Example: Multi-Protocol Setup

**Scenario**: Use different protocols for different destinations

**Sing-box Config**:
```json
{
  "inbounds": [
    {
      "type": "socks",
      "listen": "127.0.0.1",
      "listen_port": 1080
    }
  ],
  "outbounds": [
    {
      "type": "vmess",
      "tag": "vmess-us",
      "server": "us-server.example.com",
      "server_port": 443,
      "uuid": "us-uuid"
    },
    {
      "type": "trojan",
      "tag": "trojan-eu",
      "server": "eu-server.example.com",
      "server_port": 443,
      "password": "eu-password"
    },
    {
      "type": "hysteria2",
      "tag": "hy2-asia",
      "server": "asia-server.example.com",
      "server_port": 443,
      "password": "asia-password"
    }
  ],
  "route": {
    "rules": [
      {
        "geoip": "us",
        "outbound": "vmess-us"
      },
      {
        "geoip": "eu",
        "outbound": "trojan-eu"
      },
      {
        "geoip": "asia",
        "outbound": "hy2-asia"
      }
    ]
  }
}
```

**VPP Config**:
```bash
# Just one line - sing-box does the protocol routing!
singbox set endpoint 127.0.0.1:1080 socks5
singbox enable GigabitEthernet0/0/0
```

**Result**: VPP traffic automatically routed through different protocols based on destination!

---

## Conclusion

**VPP doesn't need to implement every protocol** - it just needs SOCKS5. Sing-box does all the heavy lifting of protocol conversion, giving you access to 20+ protocols without adding complexity to VPP.

This design follows the Unix philosophy: **Do one thing and do it well.**
- VPP: High-speed packet processing
- Sing-box: Protocol conversion and routing

Copyright: Internet Mastering & Company, Inc.

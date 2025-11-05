# Sing-box Integration Plugin for VPP

## Overview

The Sing-box Integration Plugin provides seamless integration between VPP (Vector Packet Processing) and [sing-box](https://github.com/SagerNet/sing-box), a universal proxy platform. This plugin enables VPP to redirect traffic to a sing-box instance for advanced protocol handling including SOCKS5, VMess, Trojan, Shadowsocks, and other proxy protocols.

## Features

- **Universal Proxy Support**: Integrate with sing-box to support multiple proxy protocols
- **Per-Interface Configuration**: Enable/disable sing-box redirection on specific interfaces
- **Flexible Endpoint Configuration**: Configure global or per-interface proxy endpoints
- **Protocol Support**: SOCKS5 and HTTP proxy protocols
- **Comprehensive Statistics**: Track packets redirected, bytes transferred, and connection failures
- **CLI Management**: Easy-to-use command-line interface for configuration
- **API Integration**: Full VPP binary API support for programmatic control

## Architecture

The plugin operates as a VPP graph node in the `ip4-unicast` feature arc. When enabled on an interface:

1. IPv4 packets are intercepted before IP lookup
2. Packets are marked for redirection to the configured sing-box endpoint
3. Statistics are updated for monitoring purposes
4. Packets continue through the VPP graph for processing

```
┌─────────────┐      ┌──────────────┐      ┌─────────────┐
│   Ethernet  │─────▶│   Singbox    │─────▶│  IP Lookup  │
│    Input    │      │     Node     │      │             │
└─────────────┘      └──────────────┘      └─────────────┘
                            │
                            ▼
                     ┌──────────────┐
                     │   Sing-box   │
                     │   Instance   │
                     └──────────────┘
```

## Building the Plugin

The plugin is automatically built with VPP when present in the source tree:

```bash
cd /path/to/vpp
make build
```

Or build specifically:

```bash
make build-release
# or for debug builds
make build-debug
```

The plugin will be installed to:
- Binary: `/usr/lib/vpp_plugins/singbox_plugin.so`
- Headers: `/usr/include/vpp_plugins/singbox/`

## Configuration

### Setting Global Endpoint

Configure a default sing-box endpoint that applies to all interfaces unless overridden:

```bash
vpp# singbox set endpoint 127.0.0.1:1080 socks5
```

For HTTP proxy:

```bash
vpp# singbox set endpoint 192.168.1.100:8080 http
```

### Enabling on an Interface

Enable sing-box on a specific interface using the default endpoint:

```bash
vpp# singbox enable GigabitEthernet0/0/0
```

Enable with a custom proxy endpoint:

```bash
vpp# singbox enable GigabitEthernet0/0/0 proxy 10.0.0.5:1080
```

### Disabling on an Interface

```bash
vpp# singbox enable GigabitEthernet0/0/0 disable
```

### Viewing Statistics

Show global configuration and all interface statistics:

```bash
vpp# show singbox
```

Show statistics for a specific interface:

```bash
vpp# show singbox GigabitEthernet0/0/0
```

Output example:

```
Interface GigabitEthernet0/0/0:
  Proxy: 127.0.0.1:1080
  Packets redirected: 1234567
  Bytes redirected: 987654321
  Connection failures: 0
```

## VPP Binary API

The plugin provides three API calls for programmatic control:

### singbox_enable_disable

Enable or disable sing-box on an interface.

**Parameters:**
- `sw_if_index`: Software interface index
- `enable_disable`: 1 to enable, 0 to disable
- `proxy_addr`: IPv4 address of proxy server
- `proxy_port`: TCP port of proxy server

### singbox_set_endpoint

Set the global default proxy endpoint.

**Parameters:**
- `proxy_addr`: IPv4 address of proxy server
- `proxy_port`: TCP port of proxy server
- `protocol_type`: 0 for SOCKS5, 1 for HTTP

### singbox_get_stats

Retrieve statistics for an interface.

**Parameters:**
- `sw_if_index`: Software interface index

**Returns:**
- `packets_redirected`: Total packets redirected
- `bytes_redirected`: Total bytes redirected
- `connection_failures`: Total connection failures

## Integration with Sing-box

### Prerequisites

1. Install and configure sing-box on your system
2. Start sing-box with appropriate configuration

Example sing-box configuration (`/etc/sing-box/config.json`):

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
      "type": "direct",
      "tag": "direct"
    },
    {
      "type": "vmess",
      "tag": "vmess-out",
      "server": "your-server.com",
      "server_port": 443,
      "uuid": "your-uuid-here",
      "security": "auto"
    }
  ]
}
```

### Starting Sing-box

```bash
# Start sing-box
sudo systemctl start sing-box

# Or run manually
sing-box run -c /etc/sing-box/config.json
```

### VPP Configuration

Once sing-box is running, configure VPP to redirect traffic:

```bash
# Connect to VPP CLI
sudo vppctl

# Set the sing-box endpoint
singbox set endpoint 127.0.0.1:1080 socks5

# Enable on desired interface
singbox enable GigabitEthernet0/0/0

# Verify configuration
show singbox
```

## Use Cases

### 1. Privacy and Anonymity

Redirect all traffic from specific interfaces through privacy-focused protocols like VMess or Shadowsocks.

### 2. Bypassing Network Restrictions

Use sing-box's protocol obfuscation capabilities to traverse restrictive networks.

### 3. Load Balancing

Distribute traffic across multiple proxy endpoints for load balancing.

### 4. Protocol Translation

Convert between different proxy protocols using sing-box as an intermediary.

### 5. Traffic Analysis

Monitor and analyze traffic patterns with sing-box's logging capabilities.

## Performance Considerations

- **Graph Node Position**: The plugin operates early in the packet processing pipeline (before IP lookup)
- **Zero-Copy Design**: Packet data is not copied, only metadata is modified
- **Per-Worker Statistics**: Statistics are accumulated without locks in the data path
- **Efficient Lookups**: Per-interface configuration uses direct vector indexing

## Troubleshooting

### Plugin Not Loading

Check VPP logs:

```bash
sudo cat /var/log/vpp/vpp.log | grep singbox
```

Verify plugin file exists:

```bash
ls -l /usr/lib/vpp_plugins/singbox_plugin.so
```

### No Traffic Being Redirected

1. Verify sing-box is running:
   ```bash
   ps aux | grep sing-box
   netstat -tlnp | grep 1080
   ```

2. Check VPP configuration:
   ```bash
   vppctl show singbox
   ```

3. Verify interface is enabled:
   ```bash
   vppctl show interface
   ```

4. Check error counters:
   ```bash
   vppctl show errors
   ```

### High Connection Failures

- Verify sing-box endpoint is reachable
- Check sing-box logs for errors
- Ensure firewall rules allow VPP to connect to sing-box
- Verify proxy configuration in sing-box

## Development Status

**Current Status**: Development

This plugin is currently in development phase. The current implementation provides:

- ✅ Basic framework and plugin registration
- ✅ CLI commands for configuration
- ✅ VPP Binary API support
- ✅ Per-interface statistics
- ✅ Graph node integration
- ⚠️ Packet encapsulation (requires implementation)
- ⚠️ SOCKS5 protocol handling (requires implementation)
- ⚠️ Connection management (requires implementation)

## Future Enhancements

Planned features for future releases:

1. **Full SOCKS5 Implementation**: Complete SOCKS5 protocol encapsulation
2. **HTTP Proxy Support**: HTTP CONNECT method implementation
3. **Connection Pooling**: Maintain persistent connections to sing-box
4. **IPv6 Support**: Extend to IPv6 traffic
5. **Rule-Based Routing**: Configure rules to selectively redirect traffic
6. **DNS Integration**: Redirect DNS queries through sing-box
7. **Performance Optimizations**: SIMD instructions for protocol processing

## Contributing

Contributions are welcome! Areas where help is needed:

- SOCKS5 protocol implementation
- Performance optimizations
- Testing and validation
- Documentation improvements

## License

Copyright (c) 2025 Internet Mastering & Company

Licensed under the Apache License, Version 2.0. See LICENSE file for details.

## References

- [Sing-box Project](https://github.com/SagerNet/sing-box)
- [VPP Documentation](https://fd.io/docs/vpp/)
- [VPP Plugin Development](https://wiki.fd.io/view/VPP/Plugin_Development)
- [SOCKS5 Protocol RFC](https://www.rfc-editor.org/rfc/rfc1928)

## Support

For issues and questions:

- VPP Issues: https://jira.fd.io/projects/vpp
- Sing-box Issues: https://github.com/SagerNet/sing-box/issues
- This Plugin: Create an issue in the VPP repository

## Acknowledgments

- VPP/FD.io community for the excellent packet processing framework
- Sing-box developers for the universal proxy platform
- Contributors to both projects

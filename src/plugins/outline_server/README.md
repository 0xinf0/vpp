# Outline Server Plugin for VPP

## Overview

The Outline Server plugin integrates [Jigsaw's Outline Shadowsocks server](https://github.com/Jigsaw-Code/outline-ss-server) with VPP (Vector Packet Processing), providing high-performance censorship circumvention capabilities within the VPP framework.

This production-grade plugin enables VPP to run a complete Shadowsocks server with advanced features including:

- **Multi-user support**: Multiple users can share a single port with independent access keys
- **Data limits**: Per-user data limit enforcement and tracking
- **Metrics**: Prometheus metrics integration for comprehensive monitoring
- **Live reload**: Configuration updates without service restart
- **Security**: Built-in replay attack defense mechanism
- **Full VPP integration**: Native CLI and Binary API support

## Architecture

The plugin manages the outline-ss-server (written in Go) as a child process while providing a comprehensive management interface through VPP's native CLI and API. This architecture allows for:

- Seamless integration with existing VPP deployments
- High-performance packet processing
- Production-grade process management
- Real-time configuration updates
- Comprehensive monitoring and statistics

## Requirements

- VPP 23.10 or later
- Go 1.20 or later (for building outline-ss-server)
- OpenSSL (typically already required by VPP)

## Building

The plugin is built automatically when building VPP from source:

```bash
# Build VPP with all plugins
make build

# Or build specific plugins including outline_server
make build-release
```

The build process automatically:
1. Compiles the outline-ss-server Go binary
2. Builds the VPP plugin (C code)
3. Installs both components to the appropriate locations

## Configuration

### Basic Setup

1. **Start the Outline server**:
```
vpp# outline-server start metrics-port 9091
```

2. **Add a listening port**:
```
vpp# outline-server add port 8388 password mySecurePassword cipher chacha20-ietf-poly1305 timeout 300
```

3. **Add user access keys**:
```
vpp# outline-server add key id user1 port-id 0 password user1Password
vpp# outline-server add key id user2 port-id 0 password user2Password data-limit 10737418240
```

### Configuration Parameters

#### Server Configuration
- **metrics-port**: Port for Prometheus metrics (default: 9091)
- **tcp-timeout**: TCP connection timeout in seconds (default: 300)
- **udp-timeout**: UDP session timeout in seconds (default: 60)
- **replay-history**: Size of replay defense history buffer (default: 10000)

#### Port Configuration
- **port**: Port number to listen on (required)
- **password**: Default password for the port (required)
- **cipher**: Encryption cipher (default: chacha20-ietf-poly1305)
  - Supported: aes-128-gcm, aes-192-gcm, aes-256-gcm, chacha20-ietf-poly1305
- **timeout**: Connection timeout in seconds (default: 300)

#### Key (User) Configuration
- **id**: Unique identifier for the access key (required)
- **port-id**: Port to associate with this key (required)
- **password**: User-specific password (required)
- **data-limit**: Data usage limit in bytes (0 = unlimited)

## CLI Commands

### Server Management

```bash
# Start the server
outline-server start [config <file>] [metrics-port <port>]

# Stop the server
outline-server stop

# Restart the server
outline-server restart

# Reload configuration (live update)
outline-server reload

# Show server status
show outline-server status
```

### Port Management

```bash
# Add a port
outline-server add port <port> password <password> [cipher <cipher>] [timeout <seconds>]

# Delete a port
outline-server delete port id <port-id>

# Show all ports
show outline-server ports
```

### Key (User) Management

```bash
# Add a key
outline-server add key id <key-id> port-id <port-id> password <password> [data-limit <bytes>]

# Delete a key
outline-server delete key id <key-id>

# Show all keys
show outline-server keys
```

### Monitoring

```bash
# Show server status and statistics
show outline-server status

# Show current configuration
show outline-server config
```

## API Usage

The plugin provides a comprehensive binary API for programmatic control. See `outline_server.api` for complete API definitions.

### Example: Python API Client

```python
from vpp_papi import VPPApiClient

# Connect to VPP
vpp = VPPApiClient(apifiles=['path/to/outline_server.api.json'])
vpp.connect('outline_client')

# Enable the server
vpp.api.outline_server_enable_disable(
    enable=True,
    metrics_port=9091,
    config_file=''
)

# Add a port
result = vpp.api.outline_server_add_port(
    port=8388,
    password='mySecurePassword',
    cipher='chacha20-ietf-poly1305',
    timeout=300
)
port_id = result.port_id

# Add a key
vpp.api.outline_server_add_key(
    key_id='user1',
    port_id=port_id,
    password='user1Password',
    data_limit=0
)

# Get statistics
stats = vpp.api.outline_server_get_stats()
print(f"Active connections: {stats.active_connections}")
print(f"Total connections: {stats.total_connections}")

# Disconnect
vpp.disconnect()
```

## Metrics

When metrics are enabled, the server exposes Prometheus metrics on the configured port (default: 9091).

### Available Metrics

- **Connection metrics**: Total/active connections, connection rate
- **Traffic metrics**: Bytes sent/received, packets sent/received
- **User metrics**: Per-key traffic and connection counts
- **Error metrics**: Authentication failures, connection errors, replay attacks blocked
- **Performance metrics**: Latency, throughput

Access metrics at: `http://<vpp-host>:9091/metrics`

## Security Considerations

### Passwords
- Use strong, unique passwords for each port and key
- Consider using password generators for production deployments
- Rotate passwords regularly

### Ciphers
- Default cipher (chacha20-ietf-poly1305) provides good security and performance
- For maximum security, consider aes-256-gcm
- Avoid deprecated ciphers

### Network Security
- Limit metrics endpoint access to trusted networks
- Consider using VPP ACLs to restrict access
- Monitor for unusual traffic patterns

### Data Limits
- Set appropriate data limits for users to prevent abuse
- Monitor data usage through metrics
- Implement alerting for limit violations

## Troubleshooting

### Server Won't Start

1. Check VPP logs:
```bash
vpp# show logging
```

2. Verify Go binary exists:
```bash
vpp# show outline-server status
```

3. Check for port conflicts:
```bash
# On the host
netstat -tuln | grep <port>
```

### Configuration Issues

1. View current configuration:
```bash
vpp# show outline-server config
```

2. Check for validation errors in logs:
```bash
vpp# show logging
```

### Performance Issues

1. Monitor statistics:
```bash
vpp# show outline-server status
```

2. Check system resources:
- CPU usage
- Memory usage
- Network bandwidth

3. Review Prometheus metrics for detailed insights

### Connection Failures

1. Verify port configuration:
```bash
vpp# show outline-server ports
```

2. Check key configuration:
```bash
vpp# show outline-server keys
```

3. Monitor authentication failures in statistics

## Performance Tuning

### VPP Configuration

Optimize VPP for high throughput:

```bash
# Increase number of worker threads
cpu {
  main-core 0
  corelist-workers 1-7
}

# Configure buffer allocation
buffers {
  buffers-per-numa 128000
}
```

### Outline Server Configuration

For high-traffic deployments:

```bash
# Increase timeouts for long-lived connections
outline-server add port 8388 password secret timeout 600

# Increase replay history for better security
# (configured through API or directly in config)
```

### System Tuning

```bash
# Increase system limits
ulimit -n 65535

# Optimize network stack
sysctl -w net.core.rmem_max=134217728
sysctl -w net.core.wmem_max=134217728
```

## Integration Examples

### With VPP NAT

```bash
# Configure NAT for outline server traffic
nat44 add interface address GigabitEthernet0/0
set interface nat44 in GigabitEthernet1/0 out GigabitEthernet0/0

# Start outline server
outline-server start
outline-server add port 8388 password secret
```

### With VPP ACLs

```bash
# Restrict outline server access
acl-plugin permit ip from 0.0.0.0/0 to 0.0.0.0/0
set acl-plugin input acl 1 GigabitEthernet1/0
```

### With Prometheus/Grafana

1. Configure Prometheus to scrape VPP metrics:

```yaml
scrape_configs:
  - job_name: 'outline-server'
    static_configs:
      - targets: ['vpp-host:9091']
```

2. Import Outline Server dashboard to Grafana
3. Set up alerts for key metrics

## Production Deployment Checklist

- [ ] VPP properly configured with adequate resources
- [ ] Strong passwords generated for all ports and keys
- [ ] Data limits configured for users
- [ ] Metrics enabled and monitored
- [ ] Logging configured and aggregated
- [ ] Alerting configured for critical metrics
- [ ] Backup configuration stored securely
- [ ] Security hardening applied (firewall, ACLs, etc.)
- [ ] Performance testing completed
- [ ] Monitoring dashboards created
- [ ] Runbook created for common issues
- [ ] Regular password rotation scheduled

## Support

For issues and questions:

- VPP issues: [FD.io mailing lists](https://fd.io/community)
- Outline server issues: [GitHub Issues](https://github.com/Jigsaw-Code/outline-ss-server/issues)
- Plugin issues: Contact Internet Mastering & Company

## License

This plugin is licensed under the Apache License 2.0, same as VPP.

The integrated outline-ss-server is also licensed under Apache License 2.0.

See LICENSE file for details.

## Contributing

Contributions are welcome! Please follow VPP's contribution guidelines:

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Add tests if applicable
5. Submit a pull request

## Acknowledgments

- The VPP community for the excellent packet processing framework
- Jigsaw team for the Outline Shadowsocks server
- Internet Mastering & Company for plugin development and maintenance

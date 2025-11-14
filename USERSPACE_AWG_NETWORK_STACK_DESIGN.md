# Minimal Userspace Network Stack Architecture for Standalone AWG VPN

**Agent 3: Minimal Network Stack Designer**  
**Date:** 2025-11-14  
**Version:** 1.0

---

## Executive Summary

This document presents a comprehensive architecture for a minimal userspace network stack designed specifically for standalone AmneziaWG (AWG) VPN operation. The design leverages lessons learned from VPP's high-performance implementation while providing a lightweight, portable alternative suitable for deployment on resource-constrained environments, mobile devices, and standard Linux/BSD systems.

### Key Design Goals

1. **Portability**: Run on Linux, macOS, iOS, Android, Windows, BSD
2. **Performance**: Achieve >1 Gbps throughput on modern hardware
3. **Minimal Dependencies**: Reduce external library requirements
4. **AWG Protocol Support**: Full AmneziaWG 1.5 compatibility (i-headers, obfuscation, junk packets)
5. **Transport Flexibility**: Support UDP, TCP, and future transports (QUIC, HTTP/2)
6. **Security**: Memory-safe implementation, minimal attack surface

---

## 1. TUN/TAP Interface Requirements

### 1.1 Linux TUN Device Management

```
┌─────────────────────────────────────────────────────────┐
│                    User Application                     │
│  (AWG VPN Client/Server)                               │
└────────────┬────────────────────────────────────────────┘
             │
             │ read()/write()
             ▼
┌─────────────────────────────────────────────────────────┐
│              TUN Device Manager                         │
│  ┌──────────────────────────────────────────────────┐  │
│  │  Device Operations:                              │  │
│  │  • tun_create()    - Allocate TUN device        │  │
│  │  • tun_destroy()   - Cleanup                     │  │
│  │  • tun_read()      - Read IP packets             │  │
│  │  • tun_write()     - Write IP packets            │  │
│  │  • tun_set_mtu()   - Configure MTU               │  │
│  │  • tun_set_addr()  - Assign IP address           │  │
│  └──────────────────────────────────────────────────┘  │
└────────────┬────────────────────────────────────────────┘
             │ ioctl()/read()/write()
             ▼
┌─────────────────────────────────────────────────────────┐
│          Kernel Network Stack                           │
│  /dev/net/tun (Linux)                                   │
│  /dev/tunX (BSD/macOS)                                  │
└─────────────────────────────────────────────────────────┘
```

#### 1.1.1 Device Creation API

```c
typedef struct {
    char name[IFNAMSIZ];        // Interface name (e.g., "awg0")
    int fd;                     // File descriptor
    int mtu;                    // Maximum transmission unit
    uint32_t flags;             // IFF_TUN | IFF_NO_PI
    struct {
        ip4_address_t ip4;      // IPv4 address
        ip6_address_t ip6;      // IPv6 address
        uint8_t prefix_len4;    // IPv4 prefix length
        uint8_t prefix_len6;    // IPv6 prefix length
    } addr;
} tun_device_t;

// Create and configure TUN device
int tun_create(tun_device_t *dev, const char *name);
int tun_destroy(tun_device_t *dev);
int tun_set_nonblocking(tun_device_t *dev);
int tun_set_mtu(tun_device_t *dev, int mtu);
int tun_set_address(tun_device_t *dev, 
                    const ip46_address_t *addr, 
                    uint8_t prefix_len);
```

#### 1.1.2 Platform-Specific Implementations

```c
// Linux implementation
#ifdef __linux__
int tun_create_linux(tun_device_t *dev, const char *name) {
    struct ifreq ifr;
    int fd, err;
    
    // Open TUN device
    if ((fd = open("/dev/net/tun", O_RDWR)) < 0) {
        return -errno;
    }
    
    memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_flags = IFF_TUN | IFF_NO_PI;  // No packet info header
    
    if (name) {
        strncpy(ifr.ifr_name, name, IFNAMSIZ);
    }
    
    // Create interface
    if ((err = ioctl(fd, TUNSETIFF, (void *) &ifr)) < 0) {
        close(fd);
        return err;
    }
    
    // Store device info
    dev->fd = fd;
    strncpy(dev->name, ifr.ifr_name, IFNAMSIZ);
    
    return 0;
}
#endif

// macOS/BSD implementation
#ifdef __APPLE__
int tun_create_macos(tun_device_t *dev, const char *name) {
    // Use utun devices: /dev/utunX
    // macOS provides built-in kernel extension
    // ...
}
#endif
```

### 1.2 Packet Read/Write Operations

#### 1.2.1 High-Performance Packet I/O

```c
typedef struct {
    uint8_t *data;              // Packet buffer
    size_t len;                 // Packet length
    uint64_t timestamp;         // Receive timestamp
    uint32_t mark;              // Fwmark (for routing)
} tun_packet_t;

// Non-blocking packet read
ssize_t tun_read_packet(tun_device_t *dev, 
                        tun_packet_t *pkt, 
                        uint8_t *buffer, 
                        size_t bufsize) {
    ssize_t n = read(dev->fd, buffer, bufsize);
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0;  // No data available
        }
        return -errno;
    }
    
    pkt->data = buffer;
    pkt->len = n;
    pkt->timestamp = get_monotonic_time_ns();
    
    return n;
}

// Batched packet write
int tun_write_packets(tun_device_t *dev, 
                      tun_packet_t *pkts, 
                      size_t count) {
    for (size_t i = 0; i < count; i++) {
        ssize_t n = write(dev->fd, pkts[i].data, pkts[i].len);
        if (n < 0) {
            return -errno;
        }
        if (n != pkts[i].len) {
            return -EIO;  // Partial write
        }
    }
    return count;
}
```

#### 1.2.2 Vectored I/O Support (Linux)

```c
#ifdef __linux__
// Use readv/writev for efficient batch operations
int tun_readv_packets(tun_device_t *dev, 
                      struct iovec *iovs, 
                      size_t count) {
    struct mmsghdr msgs[count];
    memset(msgs, 0, sizeof(msgs));
    
    for (size_t i = 0; i < count; i++) {
        msgs[i].msg_hdr.msg_iov = &iovs[i];
        msgs[i].msg_hdr.msg_iovlen = 1;
    }
    
    int n = recvmmsg(dev->fd, msgs, count, MSG_DONTWAIT, NULL);
    return n;
}
#endif
```

### 1.3 IP Address Assignment

```c
// Configure interface IP address
int tun_set_address(tun_device_t *dev, 
                    const ip46_address_t *addr, 
                    uint8_t prefix_len) {
    struct ifreq ifr;
    struct sockaddr_in *sin;
    int sockfd;
    
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) return -errno;
    
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, dev->name, IFNAMSIZ);
    
    // Set IP address
    sin = (struct sockaddr_in *)&ifr.ifr_addr;
    sin->sin_family = AF_INET;
    sin->sin_addr.s_addr = addr->ip4.as_u32;
    
    if (ioctl(sockfd, SIOCSIFADDR, &ifr) < 0) {
        close(sockfd);
        return -errno;
    }
    
    // Set netmask
    sin = (struct sockaddr_in *)&ifr.ifr_netmask;
    sin->sin_family = AF_INET;
    sin->sin_addr.s_addr = htonl(~((1 << (32 - prefix_len)) - 1));
    
    if (ioctl(sockfd, SIOCSIFNETMASK, &ifr) < 0) {
        close(sockfd);
        return -errno;
    }
    
    // Bring interface up
    if (ioctl(sockfd, SIOCGIFFLAGS, &ifr) < 0) {
        close(sockfd);
        return -errno;
    }
    
    ifr.ifr_flags |= IFF_UP | IFF_RUNNING;
    
    if (ioctl(sockfd, SIOCSIFFLAGS, &ifr) < 0) {
        close(sockfd);
        return -errno;
    }
    
    close(sockfd);
    return 0;
}
```

### 1.4 Routing Table Manipulation

```c
// Add route via TUN interface
int tun_add_route(tun_device_t *dev, 
                  const ip46_address_t *dest, 
                  uint8_t prefix_len) {
#ifdef __linux__
    // Use netlink for Linux
    struct {
        struct nlmsghdr nlh;
        struct rtmsg rtm;
        char buf[1024];
    } req;
    
    memset(&req, 0, sizeof(req));
    
    req.nlh.nlmsg_len = NLMSG_LENGTH(sizeof(struct rtmsg));
    req.nlh.nlmsg_type = RTM_NEWROUTE;
    req.nlh.nlmsg_flags = NLM_F_REQUEST | NLM_F_CREATE;
    
    req.rtm.rtm_family = AF_INET;
    req.rtm.rtm_dst_len = prefix_len;
    req.rtm.rtm_protocol = RTPROT_BOOT;
    req.rtm.rtm_scope = RT_SCOPE_LINK;
    req.rtm.rtm_type = RTN_UNICAST;
    
    // Add destination address attribute
    // Add output interface attribute
    // Send via netlink socket
    
    return 0;
#else
    // Use routing socket for BSD/macOS
    return bsd_add_route(dev, dest, prefix_len);
#endif
}
```

---

## 2. UDP/TCP Socket Management

### 2.1 Socket Abstraction Layer

```
┌─────────────────────────────────────────────────────────┐
│           Transport Abstraction Layer                   │
│  ┌──────────────────────────────────────────────────┐  │
│  │  transport_ops_t:                                │  │
│  │  • create()   - Create socket                    │  │
│  │  • bind()     - Bind to address                  │  │
│  │  • send()     - Send packet                      │  │
│  │  • recv()     - Receive packet                   │  │
│  │  • close()    - Close socket                     │  │
│  └──────────────────────────────────────────────────┘  │
└────────────┬────────────────────────────────────────────┘
             │
      ┌──────┴──────┐
      │             │
      ▼             ▼
┌──────────┐  ┌──────────┐
│   UDP    │  │   TCP    │
│ Transport│  │ Transport│
└──────────┘  └──────────┘
```

#### 2.1.1 Transport Interface Definition

```c
typedef enum {
    TRANSPORT_UDP = 0,
    TRANSPORT_TCP = 1,
    TRANSPORT_QUIC = 2,       // Future support
} transport_type_t;

typedef struct transport_ops_s {
    int (*create)(void **ctx, transport_type_t type);
    int (*bind)(void *ctx, const ip46_address_t *addr, uint16_t port);
    int (*connect)(void *ctx, const ip46_address_t *addr, uint16_t port);
    ssize_t (*send)(void *ctx, const uint8_t *data, size_t len,
                    const ip46_address_t *dst, uint16_t port);
    ssize_t (*recv)(void *ctx, uint8_t *buffer, size_t bufsize,
                    ip46_address_t *src, uint16_t *port);
    int (*close)(void *ctx);
    int (*set_nonblocking)(void *ctx);
    int (*get_fd)(void *ctx);  // For epoll/kqueue integration
} transport_ops_t;

typedef struct transport_s {
    transport_type_t type;
    const transport_ops_t *ops;
    void *ctx;
    int fd;
} transport_t;
```

### 2.2 UDP Transport Implementation

```c
typedef struct udp_transport_s {
    int sockfd;
    struct sockaddr_storage bind_addr;
    bool is_ip4;
} udp_transport_t;

static int udp_create(void **ctx, transport_type_t type) {
    udp_transport_t *udp = calloc(1, sizeof(*udp));
    if (!udp) return -ENOMEM;
    
    udp->sockfd = socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK, 0);
    if (udp->sockfd < 0) {
        free(udp);
        return -errno;
    }
    
    // Set socket options for performance
    int optval = 1;
    setsockopt(udp->sockfd, SOL_SOCKET, SO_REUSEADDR, 
               &optval, sizeof(optval));
    
    // Increase buffer sizes
    int bufsize = 4 * 1024 * 1024;  // 4 MB
    setsockopt(udp->sockfd, SOL_SOCKET, SO_RCVBUF, 
               &bufsize, sizeof(bufsize));
    setsockopt(udp->sockfd, SOL_SOCKET, SO_SNDBUF, 
               &bufsize, sizeof(bufsize));
    
    *ctx = udp;
    return 0;
}

static ssize_t udp_send(void *ctx, const uint8_t *data, size_t len,
                        const ip46_address_t *dst, uint16_t port) {
    udp_transport_t *udp = ctx;
    struct sockaddr_in dst_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(port),
        .sin_addr.s_addr = dst->ip4.as_u32,
    };
    
    return sendto(udp->sockfd, data, len, MSG_NOSIGNAL,
                  (struct sockaddr *)&dst_addr, sizeof(dst_addr));
}

static ssize_t udp_recv(void *ctx, uint8_t *buffer, size_t bufsize,
                        ip46_address_t *src, uint16_t *port) {
    udp_transport_t *udp = ctx;
    struct sockaddr_storage src_addr;
    socklen_t addrlen = sizeof(src_addr);
    
    ssize_t n = recvfrom(udp->sockfd, buffer, bufsize, 0,
                         (struct sockaddr *)&src_addr, &addrlen);
    if (n < 0) return -errno;
    
    // Extract source address and port
    if (src_addr.ss_family == AF_INET) {
        struct sockaddr_in *sin = (struct sockaddr_in *)&src_addr;
        src->ip4.as_u32 = sin->sin_addr.s_addr;
        *port = ntohs(sin->sin_port);
    }
    
    return n;
}

static const transport_ops_t udp_ops = {
    .create = udp_create,
    .bind = udp_bind,
    .send = udp_send,
    .recv = udp_recv,
    .close = udp_close,
    .set_nonblocking = udp_set_nonblocking,
    .get_fd = udp_get_fd,
};
```

### 2.3 TCP Transport Implementation

```c
typedef struct tcp_transport_s {
    int listen_sockfd;          // Listening socket
    int conn_sockfd;            // Connected socket
    struct sockaddr_storage peer_addr;
    uint8_t recv_buffer[65536]; // Accumulation buffer
    size_t recv_offset;         // Bytes in buffer
} tcp_transport_t;

// TCP framing: 2-byte length prefix (network byte order)
typedef struct tcp_frame_header_s {
    uint16_t length;            // Length of WireGuard message
} __attribute__((packed)) tcp_frame_header_t;

static ssize_t tcp_send(void *ctx, const uint8_t *data, size_t len,
                        const ip46_address_t *dst, uint16_t port) {
    tcp_transport_t *tcp = ctx;
    
    // Prepend 2-byte length header
    tcp_frame_header_t hdr = {
        .length = htons(len)
    };
    
    struct iovec iov[2] = {
        { .iov_base = &hdr, .iov_len = sizeof(hdr) },
        { .iov_base = (void *)data, .iov_len = len }
    };
    
    ssize_t n = writev(tcp->conn_sockfd, iov, 2);
    if (n < 0) return -errno;
    if (n != sizeof(hdr) + len) return -EIO;  // Partial write
    
    return len;  // Return payload length (not including header)
}

static ssize_t tcp_recv(void *ctx, uint8_t *buffer, size_t bufsize,
                        ip46_address_t *src, uint16_t *port) {
    tcp_transport_t *tcp = ctx;
    
    // Read more data into accumulation buffer
    ssize_t n = recv(tcp->conn_sockfd, 
                     tcp->recv_buffer + tcp->recv_offset,
                     sizeof(tcp->recv_buffer) - tcp->recv_offset,
                     MSG_DONTWAIT);
    
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0;  // No data available
        }
        return -errno;
    }
    
    tcp->recv_offset += n;
    
    // Check if we have a complete frame
    if (tcp->recv_offset < sizeof(tcp_frame_header_t)) {
        return 0;  // Need more data for header
    }
    
    tcp_frame_header_t *hdr = (tcp_frame_header_t *)tcp->recv_buffer;
    uint16_t msg_len = ntohs(hdr->length);
    
    size_t total_len = sizeof(tcp_frame_header_t) + msg_len;
    if (tcp->recv_offset < total_len) {
        return 0;  // Need more data for complete message
    }
    
    // We have a complete message
    if (msg_len > bufsize) {
        return -EMSGSIZE;  // Buffer too small
    }
    
    memcpy(buffer, tcp->recv_buffer + sizeof(tcp_frame_header_t), msg_len);
    
    // Remove processed data from buffer
    memmove(tcp->recv_buffer, tcp->recv_buffer + total_len,
            tcp->recv_offset - total_len);
    tcp->recv_offset -= total_len;
    
    return msg_len;
}

static const transport_ops_t tcp_ops = {
    .create = tcp_create,
    .bind = tcp_bind,
    .connect = tcp_connect,
    .send = tcp_send,
    .recv = tcp_recv,
    .close = tcp_close,
    .set_nonblocking = tcp_set_nonblocking,
    .get_fd = tcp_get_fd,
};
```

### 2.4 NAT Traversal (STUN/ICE)

```c
typedef struct stun_session_s {
    transport_t *transport;
    ip46_address_t stun_server;
    uint16_t stun_port;
    ip46_address_t public_addr;  // Discovered public address
    uint16_t public_port;        // Discovered public port
} stun_session_t;

// Simple STUN Binding Request
int stun_discover_public_endpoint(stun_session_t *session) {
    // Build STUN Binding Request
    uint8_t stun_request[20] = {
        0x00, 0x01,  // Message Type: Binding Request
        0x00, 0x00,  // Message Length: 0
        0x21, 0x12, 0xa4, 0x42,  // Magic Cookie
        // 12 bytes transaction ID (random)
    };
    
    // Generate random transaction ID
    arc4random_buf(stun_request + 8, 12);
    
    // Send STUN request
    ssize_t n = session->transport->ops->send(
        session->transport->ctx, stun_request, sizeof(stun_request),
        &session->stun_server, session->stun_port
    );
    
    if (n < 0) return n;
    
    // Receive STUN response (simplified - should have timeout/retry)
    uint8_t stun_response[512];
    ip46_address_t src;
    uint16_t src_port;
    
    n = session->transport->ops->recv(
        session->transport->ctx, stun_response, sizeof(stun_response),
        &src, &src_port
    );
    
    if (n < 0) return n;
    
    // Parse STUN response and extract MAPPED-ADDRESS
    // This is simplified - real implementation needs full STUN parsing
    // ...
    
    return 0;
}
```

---

## 3. Packet Processing Pipeline

### 3.1 Architecture Overview

```
┌────────────────────────────────────────────────────────────────┐
│                    PACKET FLOW DIAGRAM                         │
└────────────────────────────────────────────────────────────────┘

OUTBOUND (TUN → Network):
────────────────────────────────────────────────────────────────

     TUN Device
         │
         │ read() - plaintext IP packet
         ▼
    ┌─────────────────┐
    │  Input Queue    │
    └────────┬────────┘
             │
             ▼
    ┌─────────────────┐
    │ Peer Lookup     │  Find peer by destination IP
    │ (Routing Table) │  (allowed-ips matching)
    └────────┬────────┘
             │
             ▼
    ┌─────────────────┐
    │ AWG Obfuscation │  
    │ • Junk packets  │  Optional: send junk packets first
    │ • i-headers     │  Optional: send i-header chain (every 120s)
    └────────┬────────┘
             │
             ▼
    ┌─────────────────┐
    │ WireGuard       │
    │ Encryption      │  ChaCha20-Poly1305, counter increment
    │                 │  Add message_data_t header
    └────────┬────────┘
             │
             ▼
    ┌─────────────────┐
    │ AWG Header      │  Optional: prepend junk header
    │ Junk Injection  │  (random bytes of configured size)
    └────────┬────────┘
             │
             ▼
    ┌─────────────────┐
    │ Transport Layer │  
    │ • UDP: direct   │  Add UDP/TCP headers
    │ • TCP: 2-byte   │  TCP: prepend length prefix
    │   length prefix │
    └────────┬────────┘
             │
             ▼
    ┌─────────────────┐
    │ IP Layer        │  Add IP header
    │ (IPv4/IPv6)     │
    └────────┬────────┘
             │
             │ send() - encrypted packet
             ▼
    Network Socket


INBOUND (Network → TUN):
────────────────────────────────────────────────────────────────

    Network Socket
         │
         │ recv() - encrypted packet
         ▼
    ┌─────────────────┐
    │  Input Queue    │
    └────────┬────────┘
             │
             ▼
    ┌─────────────────┐
    │ Transport Parse │
    │ • UDP: extract  │  Strip transport headers
    │ • TCP: deframe  │  TCP: read length prefix, extract payload
    └────────┬────────┘
             │
             ▼
    ┌─────────────────┐
    │ AWG Detection   │
    │ • Check magic   │  Detect AWG magic headers
    │   headers       │  Strip junk header if present
    │ • Strip junk    │
    └────────┬────────┘
             │
             ▼
    ┌─────────────────┐
    │ Message Type    │
    │ Classification  │  MESSAGE_DATA vs MESSAGE_HANDSHAKE_*
    └────────┬────────┘
             │
      ┌──────┴──────┐
      │             │
      ▼             ▼
┌──────────┐  ┌──────────┐
│Handshake │  │   Data   │
│Processing│  │Decryption│  ChaCha20-Poly1305
└────┬─────┘  └────┬─────┘
     │             │
     │             ▼
     │        ┌─────────────────┐
     │        │ Counter Check   │  Replay protection
     │        │ (Anti-Replay)   │
     │        └────────┬────────┘
     │                 │
     │                 ▼
     │        ┌─────────────────┐
     │        │ Allowed-IP      │  Verify source IP allowed
     │        │ Validation      │
     │        └────────┬────────┘
     │                 │
     └────────┬────────┘
              │
              │ write() - plaintext IP packet
              ▼
         TUN Device
```

### 3.2 Queue-Based Processing Model

```c
typedef struct packet_queue_s {
    struct packet_entry_s {
        uint8_t *data;
        size_t len;
        uint64_t timestamp;
        void *metadata;
        struct packet_entry_s *next;
    } *head, *tail;
    
    size_t count;
    size_t max_count;
    pthread_mutex_t lock;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
} packet_queue_t;

// Lock-free ring buffer alternative (for single producer/consumer)
typedef struct lockfree_queue_s {
    struct packet_entry_s *entries;
    size_t capacity;
    _Atomic size_t head;  // Producer index
    _Atomic size_t tail;  // Consumer index
    uint8_t _padding[128 - sizeof(_Atomic size_t) * 2];  // Cache line
} lockfree_queue_t;

int queue_enqueue(packet_queue_t *q, uint8_t *data, size_t len) {
    pthread_mutex_lock(&q->lock);
    
    while (q->count >= q->max_count) {
        pthread_cond_wait(&q->not_full, &q->lock);
    }
    
    struct packet_entry_s *entry = malloc(sizeof(*entry));
    entry->data = malloc(len);
    memcpy(entry->data, data, len);
    entry->len = len;
    entry->timestamp = get_monotonic_time_ns();
    entry->next = NULL;
    
    if (q->tail) {
        q->tail->next = entry;
    } else {
        q->head = entry;
    }
    q->tail = entry;
    q->count++;
    
    pthread_cond_signal(&q->not_empty);
    pthread_mutex_unlock(&q->lock);
    
    return 0;
}
```

### 3.3 AWG Obfuscation Layer Insertion Points

```c
typedef struct awg_processor_s {
    wg_awg_cfg_t config;        // AWG configuration
    uint64_t last_special_hs;   // Last special handshake time
    uint32_t junk_counter;      // Counter for junk packets
} awg_processor_t;

// Process outbound packet with AWG obfuscation
int awg_process_outbound(awg_processor_t *awg, 
                         uint8_t *packet, size_t *len,
                         message_type_t msg_type) {
    if (!awg->config.enabled) {
        return 0;  // AWG disabled, pass through
    }
    
    uint64_t now = get_monotonic_time_ns();
    
    // 1. Send i-header chain every 120 seconds (special handshake)
    if (msg_type == MESSAGE_HANDSHAKE_INITIATION &&
        (now - awg->last_special_hs) >= AWG_SPECIAL_HANDSHAKE_INTERVAL_NS) {
        
        awg_send_i_header_chain(awg);
        awg->last_special_hs = now;
    }
    
    // 2. Send junk packets before message (if configured)
    if (awg->config.junk_packet_count > 0) {
        awg_send_junk_packets(awg, awg->config.junk_packet_count);
    }
    
    // 3. Replace message type with magic header
    message_header_t *hdr = (message_header_t *)packet;
    hdr->type = wg_awg_get_magic_header(&awg->config, msg_type);
    
    // 4. Prepend junk header
    uint32_t junk_size = wg_awg_get_header_junk_size(&awg->config, msg_type);
    if (junk_size > 0) {
        // Shift packet data to make room for junk
        memmove(packet + junk_size, packet, *len);
        
        // Fill junk area with random data
        wg_awg_generate_junk(packet, junk_size);
        
        *len += junk_size;
    }
    
    return 0;
}

// Process inbound packet with AWG deobfuscation
int awg_process_inbound(awg_processor_t *awg,
                        uint8_t *packet, size_t *len) {
    if (!awg->config.enabled) {
        return 0;  // AWG disabled, pass through
    }
    
    // 1. Check if this is an i-header packet (special handshake)
    if (awg_is_i_header_packet(awg, packet, *len)) {
        // Process and discard i-header packet
        return -EAGAIN;  // Signal to drop packet
    }
    
    // 2. Detect message type from magic header
    message_header_t *hdr = (message_header_t *)packet;
    message_type_t true_type = wg_awg_get_message_type(&awg->config, 
                                                        hdr->type);
    
    if (true_type == MESSAGE_INVALID) {
        return -EINVAL;  // Invalid magic header
    }
    
    // 3. Restore original message type
    hdr->type = true_type;
    
    // 4. Strip junk header
    uint32_t junk_size = wg_awg_get_header_junk_size(&awg->config, true_type);
    if (junk_size > 0) {
        // Shift packet data to remove junk
        memmove(packet, packet + junk_size, *len - junk_size);
        *len -= junk_size;
    }
    
    return 0;
}
```

### 3.4 Crypto Operation Pipeline

```c
typedef struct crypto_op_s {
    enum {
        CRYPTO_OP_ENCRYPT,
        CRYPTO_OP_DECRYPT,
    } op_type;
    
    uint8_t *input;
    size_t input_len;
    uint8_t *output;
    size_t output_len;
    
    uint8_t key[32];            // ChaCha20 key
    uint8_t nonce[12];          // Nonce (4 zero bytes + 8 byte counter)
    uint8_t tag[16];            // Poly1305 authentication tag
    
    void (*callback)(struct crypto_op_s *op, int status);
    void *user_data;
} crypto_op_t;

// Synchronous crypto operation
int crypto_process_sync(crypto_op_t *op) {
    if (op->op_type == CRYPTO_OP_ENCRYPT) {
        // ChaCha20-Poly1305 encryption
        chacha20_poly1305_encrypt(
            op->output,          // ciphertext output
            op->tag,             // authentication tag output
            op->input,           // plaintext input
            op->input_len,       // plaintext length
            NULL,                // AAD (not used by WireGuard)
            0,                   // AAD length
            op->key,             // 256-bit key
            op->nonce            // 96-bit nonce
        );
        op->output_len = op->input_len + 16;  // Include auth tag
    } else {
        // ChaCha20-Poly1305 decryption
        int result = chacha20_poly1305_decrypt(
            op->output,          // plaintext output
            op->input,           // ciphertext input
            op->input_len - 16,  // ciphertext length (without tag)
            op->tag,             // authentication tag
            NULL,                // AAD
            0,                   // AAD length
            op->key,             // 256-bit key
            op->nonce            // 96-bit nonce
        );
        
        if (result != 0) {
            return -EBADMSG;  // Authentication failed
        }
        
        op->output_len = op->input_len - 16;
    }
    
    return 0;
}
```

---

## 4. Threading Model

### 4.1 Single-Threaded Event Loop (libuv/libev)

```
┌────────────────────────────────────────────────────────┐
│              Main Event Loop (libuv)                   │
│                                                        │
│  ┌──────────────────────────────────────────────────┐ │
│  │  Event Sources:                                  │ │
│  │  • TUN device (read events)                      │ │
│  │  • UDP socket (read/write events)                │ │
│  │  • TCP socket (read/write/accept events)         │ │
│  │  • Timer events (keepalive, handshake)           │ │
│  │  • Signal events (SIGINT, SIGTERM)               │ │
│  └──────────────────────────────────────────────────┘ │
│                                                        │
│  ┌──────────────────────────────────────────────────┐ │
│  │  Callbacks:                                      │ │
│  │  • on_tun_read()   → encrypt → send to network  │ │
│  │  • on_socket_read()→ decrypt → write to TUN     │ │
│  │  • on_timer()      → send keepalive/handshake   │ │
│  └──────────────────────────────────────────────────┘ │
└────────────────────────────────────────────────────────┘
```

#### 4.1.1 libuv Implementation

```c
#include <uv.h>

typedef struct awg_context_s {
    uv_loop_t *loop;
    uv_poll_t tun_poll;
    uv_poll_t socket_poll;
    uv_timer_t keepalive_timer;
    
    tun_device_t tun;
    transport_t transport;
    awg_processor_t awg;
    
    // Peers and routing
    wg_peer_t *peers;
    size_t peer_count;
} awg_context_t;

void on_tun_readable(uv_poll_t *handle, int status, int events) {
    awg_context_t *ctx = handle->data;
    
    uint8_t buffer[2048];
    tun_packet_t pkt;
    
    ssize_t n = tun_read_packet(&ctx->tun, &pkt, buffer, sizeof(buffer));
    if (n <= 0) return;
    
    // Find destination peer
    wg_peer_t *peer = find_peer_by_ip(&ctx->peers, ctx->peer_count, 
                                      pkt.data, pkt.len);
    if (!peer) {
        return;  // No matching peer
    }
    
    // Encrypt packet
    uint8_t encrypted[2048];
    size_t encrypted_len;
    wg_encrypt_packet(peer, pkt.data, pkt.len, 
                      encrypted, &encrypted_len);
    
    // Apply AWG obfuscation
    awg_process_outbound(&ctx->awg, encrypted, &encrypted_len, 
                         MESSAGE_DATA);
    
    // Send over network
    ctx->transport.ops->send(ctx->transport.ctx, encrypted, encrypted_len,
                             &peer->endpoint.addr, peer->endpoint.port);
}

void on_socket_readable(uv_poll_t *handle, int status, int events) {
    awg_context_t *ctx = handle->data;
    
    uint8_t buffer[2048];
    ip46_address_t src;
    uint16_t src_port;
    
    ssize_t n = ctx->transport.ops->recv(ctx->transport.ctx, buffer, 
                                         sizeof(buffer), &src, &src_port);
    if (n <= 0) return;
    
    // Apply AWG deobfuscation
    size_t len = n;
    if (awg_process_inbound(&ctx->awg, buffer, &len) < 0) {
        return;  // Drop packet (e.g., i-header)
    }
    
    // Find peer by source address
    wg_peer_t *peer = find_peer_by_endpoint(&ctx->peers, ctx->peer_count,
                                            &src, src_port);
    if (!peer) return;
    
    // Check message type
    message_header_t *hdr = (message_header_t *)buffer;
    
    if (hdr->type == MESSAGE_DATA) {
        // Decrypt data packet
        uint8_t plaintext[2048];
        size_t plaintext_len;
        
        if (wg_decrypt_packet(peer, buffer, len, 
                              plaintext, &plaintext_len) < 0) {
            return;  // Decryption failed
        }
        
        // Write to TUN
        tun_packet_t pkt = {
            .data = plaintext,
            .len = plaintext_len,
        };
        tun_write_packets(&ctx->tun, &pkt, 1);
        
    } else {
        // Handle handshake message
        wg_process_handshake(peer, buffer, len);
    }
}

void on_keepalive_timer(uv_timer_t *handle) {
    awg_context_t *ctx = handle->data;
    
    // Send keepalive to all peers if needed
    for (size_t i = 0; i < ctx->peer_count; i++) {
        wg_peer_t *peer = &ctx->peers[i];
        
        if (peer->persistent_keepalive > 0) {
            uint64_t now = get_monotonic_time_ns();
            if (now - peer->last_sent_packet >= 
                peer->persistent_keepalive * 1000000000ULL) {
                
                wg_send_keepalive(ctx, peer);
            }
        }
    }
}

int main(int argc, char **argv) {
    awg_context_t ctx = {0};
    
    ctx.loop = uv_default_loop();
    
    // Initialize TUN device
    tun_create(&ctx.tun, "awg0");
    tun_set_nonblocking(&ctx.tun);
    
    // Initialize transport
    transport_create(&ctx.transport, TRANSPORT_UDP);
    ctx.transport.ops->bind(ctx.transport.ctx, NULL, 51820);
    
    // Set up TUN polling
    uv_poll_init(ctx.loop, &ctx.tun_poll, ctx.tun.fd);
    ctx.tun_poll.data = &ctx;
    uv_poll_start(&ctx.tun_poll, UV_READABLE, on_tun_readable);
    
    // Set up socket polling
    int sockfd = ctx.transport.ops->get_fd(ctx.transport.ctx);
    uv_poll_init(ctx.loop, &ctx.socket_poll, sockfd);
    ctx.socket_poll.data = &ctx;
    uv_poll_start(&ctx.socket_poll, UV_READABLE, on_socket_readable);
    
    // Set up keepalive timer (every 10 seconds)
    uv_timer_init(ctx.loop, &ctx.keepalive_timer);
    ctx.keepalive_timer.data = &ctx;
    uv_timer_start(&ctx.keepalive_timer, on_keepalive_timer, 10000, 10000);
    
    // Run event loop
    return uv_run(ctx.loop, UV_RUN_DEFAULT);
}
```

### 4.2 Multi-Threaded Pipeline Model

```
┌────────────────────────────────────────────────────────────────┐
│                   Multi-Threaded Architecture                  │
└────────────────────────────────────────────────────────────────┘

┌──────────────┐      ┌──────────────┐      ┌──────────────┐
│  TUN Reader  │─────▶│   Crypto     │─────▶│   Network    │
│   Thread     │      │   Thread     │      │   Sender     │
│              │      │   Pool       │      │   Thread     │
└──────────────┘      └──────────────┘      └──────────────┘
       │                     ▲                     │
       │                     │                     │
       │              Lock-Free Queues             │
       │                     │                     │
       ▼                     ▼                     ▼
┌──────────────┐      ┌──────────────┐      ┌──────────────┐
│   Network    │─────▶│   Crypto     │─────▶│  TUN Writer  │
│   Receiver   │      │   Thread     │      │   Thread     │
│   Thread     │      │   Pool       │      │              │
└──────────────┘      └──────────────┘      └──────────────┘
```

#### 4.2.1 Thread Pool Implementation

```c
typedef struct thread_pool_s {
    pthread_t *threads;
    size_t thread_count;
    
    packet_queue_t input_queue;
    packet_queue_t output_queue;
    
    void (*worker_fn)(void *arg);
    void *worker_arg;
    
    _Atomic bool shutdown;
} thread_pool_t;

void *crypto_worker(void *arg) {
    thread_pool_t *pool = arg;
    
    while (!atomic_load(&pool->shutdown)) {
        struct packet_entry_s *entry = queue_dequeue(&pool->input_queue);
        if (!entry) {
            usleep(100);  // Sleep briefly if no work
            continue;
        }
        
        // Perform crypto operation
        crypto_op_t op = {
            .op_type = (int)(uintptr_t)entry->metadata,
            .input = entry->data,
            .input_len = entry->len,
        };
        
        uint8_t output[2048];
        op.output = output;
        
        crypto_process_sync(&op);
        
        // Enqueue result
        queue_enqueue(&pool->output_queue, op.output, op.output_len);
        
        // Free input entry
        free(entry->data);
        free(entry);
    }
    
    return NULL;
}

int thread_pool_create(thread_pool_t *pool, size_t thread_count,
                       void (*worker_fn)(void *), void *arg) {
    pool->threads = calloc(thread_count, sizeof(pthread_t));
    pool->thread_count = thread_count;
    pool->worker_fn = worker_fn;
    pool->worker_arg = arg;
    atomic_store(&pool->shutdown, false);
    
    queue_init(&pool->input_queue, 1024);
    queue_init(&pool->output_queue, 1024);
    
    for (size_t i = 0; i < thread_count; i++) {
        pthread_create(&pool->threads[i], NULL, worker_fn, pool);
    }
    
    return 0;
}
```

### 4.3 Lock-Free Queue Implementation

```c
// Single-producer, single-consumer lock-free ring buffer
typedef struct spsc_queue_s {
    struct entry_s {
        uint8_t *data;
        size_t len;
        void *metadata;
    } *entries;
    
    size_t capacity;
    size_t mask;  // capacity - 1 (for fast modulo)
    
    char _pad1[128];  // Cache line padding
    _Atomic size_t head;  // Write index
    char _pad2[128];
    _Atomic size_t tail;  // Read index
    char _pad3[128];
} spsc_queue_t;

int spsc_queue_init(spsc_queue_t *q, size_t capacity) {
    // Capacity must be power of 2
    if ((capacity & (capacity - 1)) != 0) {
        return -EINVAL;
    }
    
    q->entries = calloc(capacity, sizeof(struct entry_s));
    q->capacity = capacity;
    q->mask = capacity - 1;
    atomic_store(&q->head, 0);
    atomic_store(&q->tail, 0);
    
    return 0;
}

bool spsc_queue_enqueue(spsc_queue_t *q, uint8_t *data, size_t len,
                        void *metadata) {
    size_t head = atomic_load_explicit(&q->head, memory_order_relaxed);
    size_t next_head = (head + 1) & q->mask;
    
    // Check if queue is full
    size_t tail = atomic_load_explicit(&q->tail, memory_order_acquire);
    if (next_head == tail) {
        return false;  // Queue full
    }
    
    // Write entry
    q->entries[head].data = data;
    q->entries[head].len = len;
    q->entries[head].metadata = metadata;
    
    // Update head
    atomic_store_explicit(&q->head, next_head, memory_order_release);
    
    return true;
}

bool spsc_queue_dequeue(spsc_queue_t *q, uint8_t **data, size_t *len,
                        void **metadata) {
    size_t tail = atomic_load_explicit(&q->tail, memory_order_relaxed);
    size_t head = atomic_load_explicit(&q->head, memory_order_acquire);
    
    // Check if queue is empty
    if (tail == head) {
        return false;  // Queue empty
    }
    
    // Read entry
    *data = q->entries[tail].data;
    *len = q->entries[tail].len;
    *metadata = q->entries[tail].metadata;
    
    // Update tail
    size_t next_tail = (tail + 1) & q->mask;
    atomic_store_explicit(&q->tail, next_tail, memory_order_release);
    
    return true;
}
```

### 4.4 Timer Thread for Keepalives

```c
typedef struct timer_thread_s {
    pthread_t thread;
    _Atomic bool shutdown;
    
    struct timer_event_s {
        uint64_t expiry_time;
        void (*callback)(void *arg);
        void *arg;
        struct timer_event_s *next;
    } *timer_list;
    
    pthread_mutex_t lock;
    pthread_cond_t cond;
} timer_thread_t;

void *timer_thread_fn(void *arg) {
    timer_thread_t *timer = arg;
    
    while (!atomic_load(&timer->shutdown)) {
        pthread_mutex_lock(&timer->lock);
        
        uint64_t now = get_monotonic_time_ns();
        struct timer_event_s *event = timer->timer_list;
        
        // Find expired timers
        while (event && event->expiry_time <= now) {
            // Remove from list
            timer->timer_list = event->next;
            
            // Unlock before calling callback
            pthread_mutex_unlock(&timer->lock);
            
            // Execute callback
            event->callback(event->arg);
            
            free(event);
            
            pthread_mutex_lock(&timer->lock);
            event = timer->timer_list;
        }
        
        // Calculate sleep time
        struct timespec timeout;
        if (event) {
            uint64_t sleep_ns = event->expiry_time - now;
            timeout.tv_sec = sleep_ns / 1000000000ULL;
            timeout.tv_nsec = sleep_ns % 1000000000ULL;
            
            pthread_cond_timedwait(&timer->cond, &timer->lock, &timeout);
        } else {
            // No timers, wait indefinitely
            pthread_cond_wait(&timer->cond, &timer->lock);
        }
        
        pthread_mutex_unlock(&timer->lock);
    }
    
    return NULL;
}

int timer_add(timer_thread_t *timer, uint64_t delay_ns,
              void (*callback)(void *), void *arg) {
    struct timer_event_s *event = malloc(sizeof(*event));
    event->expiry_time = get_monotonic_time_ns() + delay_ns;
    event->callback = callback;
    event->arg = arg;
    
    pthread_mutex_lock(&timer->lock);
    
    // Insert in sorted order
    struct timer_event_s **prev = &timer->timer_list;
    while (*prev && (*prev)->expiry_time < event->expiry_time) {
        prev = &(*prev)->next;
    }
    
    event->next = *prev;
    *prev = event;
    
    // Wake up timer thread
    pthread_cond_signal(&timer->cond);
    pthread_mutex_unlock(&timer->lock);
    
    return 0;
}
```

---

## 5. Implementation Approach Comparison

### 5.1 Pure Rust with Tokio (Async)

**Pros:**
- Memory safety guarantees
- Zero-cost abstractions
- Excellent async/await ecosystem
- Cross-platform support
- Type safety prevents many bugs

**Cons:**
- Larger binary size compared to C
- Learning curve for Rust ownership model
- Crypto library dependencies (ring, chacha20poly1305)
- Async runtime overhead

**Architecture Sketch:**

```rust
use tokio::net::UdpSocket;
use tokio::sync::mpsc;
use tun::AsyncDevice;

struct AwgRuntime {
    tun: AsyncDevice,
    socket: UdpSocket,
    peers: HashMap<PublicKey, Peer>,
}

impl AwgRuntime {
    async fn run(&mut self) -> Result<()> {
        let (tx_encrypt, mut rx_encrypt) = mpsc::channel(1024);
        let (tx_decrypt, mut rx_decrypt) = mpsc::channel(1024);
        
        // Spawn tasks
        let tun_task = tokio::spawn(self.tun_reader(tx_encrypt));
        let socket_task = tokio::spawn(self.socket_reader(tx_decrypt));
        let encrypt_task = tokio::spawn(self.encryptor(rx_encrypt));
        let decrypt_task = tokio::spawn(self.decryptor(rx_decrypt));
        
        // Wait for all tasks
        tokio::try_join!(tun_task, socket_task, encrypt_task, decrypt_task)?;
        
        Ok(())
    }
    
    async fn tun_reader(&self, tx: mpsc::Sender<Packet>) {
        let mut buf = [0u8; 2048];
        loop {
            match self.tun.read(&mut buf).await {
                Ok(n) => {
                    let pkt = Packet::new(&buf[..n]);
                    tx.send(pkt).await.unwrap();
                }
                Err(e) => eprintln!("TUN read error: {}", e),
            }
        }
    }
    
    async fn encryptor(&self, mut rx: mpsc::Receiver<Packet>) {
        while let Some(pkt) = rx.recv().await {
            // Find peer
            let peer = self.find_peer_by_ip(&pkt.dst_ip);
            
            // Encrypt
            let encrypted = peer.encrypt(&pkt.data);
            
            // Send
            self.socket.send_to(&encrypted, peer.endpoint).await.unwrap();
        }
    }
}
```

**Performance:** ~800 Mbps - 1.2 Gbps (comparable to wireguard-go)  
**Binary Size:** ~2-3 MB (release build, stripped)  
**Platform Support:** Linux, macOS, Windows, iOS, Android, BSD

### 5.2 C with epoll/kqueue

**Pros:**
- Smallest binary size (~100-200 KB)
- Maximum performance potential
- Minimal dependencies
- Direct system call access
- Proven in WireGuard kernel module

**Cons:**
- Memory safety responsibility on developer
- Manual memory management
- Platform-specific code for epoll (Linux) vs kqueue (BSD/macOS)
- Verbose error handling

**Architecture Sketch:**

```c
typedef struct {
    int epoll_fd;
    int tun_fd;
    int socket_fd;
    
    struct peer_s *peers;
    size_t peer_count;
} awg_runtime_t;

int awg_run(awg_runtime_t *runtime) {
    struct epoll_event events[MAX_EVENTS];
    
    // Register file descriptors
    struct epoll_event ev = {0};
    ev.events = EPOLLIN;
    
    ev.data.fd = runtime->tun_fd;
    epoll_ctl(runtime->epoll_fd, EPOLL_CTL_ADD, runtime->tun_fd, &ev);
    
    ev.data.fd = runtime->socket_fd;
    epoll_ctl(runtime->epoll_fd, EPOLL_CTL_ADD, runtime->socket_fd, &ev);
    
    // Main event loop
    while (1) {
        int nfds = epoll_wait(runtime->epoll_fd, events, MAX_EVENTS, 
                              1000 /* 1 second timeout */);
        
        for (int i = 0; i < nfds; i++) {
            if (events[i].data.fd == runtime->tun_fd) {
                handle_tun_read(runtime);
            } else if (events[i].data.fd == runtime->socket_fd) {
                handle_socket_read(runtime);
            }
        }
        
        // Process timers
        process_timers(runtime);
    }
    
    return 0;
}
```

**Performance:** ~1.5-2 Gbps (best case, optimized)  
**Binary Size:** ~100-200 KB (stripped)  
**Platform Support:** Linux (epoll), BSD/macOS (kqueue - requires separate implementation)

### 5.3 Hybrid Approach (C + Rust Crypto)

**Pros:**
- C for I/O and event loop (minimal overhead)
- Rust for crypto (memory safety where it matters)
- Best of both worlds
- Can use audited crypto libraries (ring, sodiumoxide)

**Cons:**
- FFI overhead (minimal but measurable)
- Two build systems (Cargo + Make)
- Increased complexity

**Architecture:**

```c
// C side - I/O and event loop
int main(int argc, char **argv) {
    awg_runtime_t runtime;
    init_runtime(&runtime);
    
    // Event loop in C
    while (1) {
        struct packet_s pkt;
        if (tun_read(&runtime.tun, &pkt) > 0) {
            // Call Rust crypto via FFI
            uint8_t encrypted[2048];
            size_t encrypted_len;
            
            rust_encrypt_packet(pkt.data, pkt.len, 
                                encrypted, &encrypted_len);
            
            socket_send(&runtime.socket, encrypted, encrypted_len);
        }
    }
}
```

```rust
// Rust side - Crypto operations
#[no_mangle]
pub extern "C" fn rust_encrypt_packet(
    plaintext: *const u8,
    plaintext_len: usize,
    ciphertext: *mut u8,
    ciphertext_len: *mut usize,
) -> i32 {
    let plaintext_slice = unsafe {
        std::slice::from_raw_parts(plaintext, plaintext_len)
    };
    
    // Perform encryption using ring/chacha20poly1305
    let mut cipher = ChaCha20Poly1305::new(&key);
    let result = cipher.encrypt(&nonce, plaintext_slice).unwrap();
    
    unsafe {
        std::ptr::copy_nonoverlapping(
            result.as_ptr(),
            ciphertext,
            result.len()
        );
        *ciphertext_len = result.len();
    }
    
    0
}
```

**Performance:** ~1.2-1.5 Gbps  
**Binary Size:** ~500 KB - 1 MB  
**Platform Support:** Linux, macOS, Windows, BSD (best cross-platform)

---

## 6. Recommended Architecture

### 6.1 Final Recommendation: **Hybrid C + Rust**

**Rationale:**

1. **Performance**: C event loop provides lowest latency I/O
2. **Safety**: Rust crypto reduces risk of cryptographic vulnerabilities
3. **Portability**: Can target all platforms with minimal platform-specific code
4. **Maintainability**: Clear separation between I/O (C) and crypto (Rust)
5. **Auditability**: Use well-audited crypto libraries (ring, RustCrypto)

### 6.2 Module Structure

```
awg-vpn/
├── src/
│   ├── main.c                 # Entry point, CLI parsing
│   ├── runtime.c/h            # Event loop, I/O multiplexing
│   ├── tun.c/h                # TUN device management
│   ├── transport.c/h          # UDP/TCP abstraction
│   ├── peer.c/h               # Peer management
│   ├── awg.c/h                # AWG obfuscation layer
│   └── ffi.c/h                # Rust FFI bindings
│
├── crypto/                    # Rust crypto library
│   ├── Cargo.toml
│   └── src/
│       ├── lib.rs             # FFI exports
│       ├── noise.rs           # Noise protocol
│       ├── crypto.rs          # ChaCha20-Poly1305
│       └── keys.rs            # Key management
│
├── platform/
│   ├── linux.c/h              # Linux-specific (epoll, netlink)
│   ├── macos.c/h              # macOS-specific (kqueue)
│   ├── windows.c/h            # Windows-specific (IOCP)
│   └── bsd.c/h                # BSD-specific (kqueue)
│
└── tests/
    ├── unit/
    └── integration/
```

### 6.3 Build System

```makefile
# Makefile
CC = clang
CFLAGS = -O3 -Wall -Wextra -std=c11
LDFLAGS = -Lcrypto/target/release -lawg_crypto

all: build-rust build-c

build-rust:
	cd crypto && cargo build --release

build-c: $(wildcard src/*.c) $(wildcard platform/*.c)
	$(CC) $(CFLAGS) -o awg-vpn $^ $(LDFLAGS)

install:
	cp awg-vpn /usr/local/bin/
	cp crypto/target/release/libawg_crypto.so /usr/local/lib/

clean:
	rm -f awg-vpn
	cd crypto && cargo clean
```

---

## 7. Performance Benchmarks

### 7.1 Expected Throughput

| Implementation | Platform | CPU | Throughput | Latency |
|---------------|----------|-----|-----------|---------|
| VPP (kernel bypass) | Linux | Xeon E5-2680 v4 | 5-10 Gbps | < 50 µs |
| Hybrid C+Rust | Linux | i7-10700K | 1.5 Gbps | ~100 µs |
| Pure Rust (Tokio) | Linux | i7-10700K | 1.2 Gbps | ~150 µs |
| Pure C (epoll) | Linux | i7-10700K | 2.0 Gbps | ~80 µs |
| WireGuard kernel | Linux | i7-10700K | 3-4 Gbps | ~50 µs |
| wireguard-go | Linux | i7-10700K | 800 Mbps | ~200 µs |

### 7.2 Memory Usage

| Implementation | Baseline | Per Peer | 1000 Peers |
|---------------|----------|----------|------------|
| Hybrid C+Rust | 2 MB | 4 KB | 6 MB |
| Pure Rust | 5 MB | 8 KB | 13 MB |
| Pure C | 1 MB | 2 KB | 3 MB |
| VPP | 50 MB | 10 KB | 60 MB |

---

## 8. Implementation Roadmap

### Phase 1: Core Functionality (2-3 weeks)
- [ ] TUN device management (Linux)
- [ ] UDP transport
- [ ] Basic WireGuard handshake (Noise_IKpsk2)
- [ ] ChaCha20-Poly1305 crypto (Rust FFI)
- [ ] Single-threaded event loop (epoll)
- [ ] Basic peer management

### Phase 2: AWG Obfuscation (1-2 weeks)
- [ ] Magic header substitution
- [ ] Junk header injection
- [ ] Junk packet generation
- [ ] i-header chain (i1-i5)
- [ ] Special handshake timer

### Phase 3: TCP Transport (1-2 weeks)
- [ ] TCP framing (2-byte length prefix)
- [ ] TCP connection management
- [ ] Connection pooling
- [ ] Fallback logic (UDP → TCP)

### Phase 4: Platform Support (2-3 weeks)
- [ ] macOS support (kqueue, utun)
- [ ] Windows support (IOCP, wintun)
- [ ] BSD support (kqueue)
- [ ] Android support (VpnService)
- [ ] iOS support (NetworkExtension)

### Phase 5: Performance Optimization (2-3 weeks)
- [ ] Multi-threaded pipeline
- [ ] Lock-free queues
- [ ] Batched I/O (sendmmsg/recvmmsg)
- [ ] SIMD optimizations (ChaCha20)
- [ ] Zero-copy packet processing

### Phase 6: Production Hardening (2-3 weeks)
- [ ] Comprehensive error handling
- [ ] Logging and metrics
- [ ] Configuration file support
- [ ] Systemd integration
- [ ] Documentation

---

## 9. Testing Strategy

### 9.1 Unit Tests
- TUN device operations
- Crypto primitives
- AWG obfuscation/deobfuscation
- Packet framing/deframing
- Queue operations

### 9.2 Integration Tests
- End-to-end connectivity
- Handshake completion
- Data transfer
- Peer roaming
- Connection recovery

### 9.3 Performance Tests
- Throughput benchmarking
- Latency measurement
- CPU profiling
- Memory profiling
- Packet loss under load

### 9.4 Security Tests
- Fuzzing (AFL, libFuzzer)
- Static analysis (scan-build, clippy)
- Memory safety (Valgrind, AddressSanitizer)
- Timing attack resistance
- Replay attack prevention

---

## 10. References

### Research Papers
1. WireGuard Protocol Whitepaper - Jason A. Donenfeld
2. The Noise Protocol Framework - Trevor Perrin
3. ChaCha20 and Poly1305 for IETF Protocols - RFC 8439

### Existing Implementations
1. **wireguard-go**: https://git.zx2c4.com/wireguard-go
2. **BoringTun**: https://github.com/cloudflare/boringtun
3. **wireguard-rs**: https://git.zx2c4.com/wireguard-rs
4. **VPP WireGuard Plugin**: (Current repository)

### Libraries and Tools
1. **libuv**: Cross-platform async I/O - https://libuv.org
2. **Tokio**: Async runtime for Rust - https://tokio.rs
3. **ring**: Crypto library for Rust - https://github.com/briansmith/ring
4. **libsodium**: Crypto library for C - https://libsodium.org

---

## 11. Conclusion

This architecture provides a comprehensive blueprint for building a minimal, high-performance userspace network stack for standalone AWG VPN. The hybrid C + Rust approach balances performance, safety, and maintainability while supporting full AWG protocol features including obfuscation, multiple transports, and cross-platform deployment.

**Key Advantages:**
- Portable across all major platforms
- Performance approaching kernel implementations
- Memory-safe crypto operations
- Full AWG 1.5 compatibility
- Extensible transport layer

**Next Steps:**
1. Validate architecture with proof-of-concept implementation
2. Benchmark against wireguard-go and BoringTun
3. Begin Phase 1 implementation
4. Establish CI/CD pipeline
5. Create comprehensive test suite

---

**Document Version:** 1.0  
**Last Updated:** 2025-11-14  
**Author:** Agent 3 (Minimal Network Stack Designer)  
**License:** Apache 2.0

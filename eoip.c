#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <linux/if.h>
#include <linux/if_tun.h>

#include <netinet/ip.h>
#include <arpa/inet.h>
#include <errno.h>

#define MAX(a,b) (((a)>(b))?(a):(b))
#define GRE_MAGIC "\x20\x01\x64\x00"
#define EIPHEAD(tid) 0x3000 | tid
#define BITSWAP(c) ((c & 0xf0) >> 4) | ((c & 0x0f) << 4);

struct eoip6_packet {
  uint8_t head_p1;
  uint8_t head_p2;
  uint8_t payload[0];
};

struct eoip_packet {
  uint8_t  magic[4];
  uint16_t len;
  uint16_t tid;
  uint8_t  payload[0];
};

void populate_sockaddr(int af, int port, char addr[],
  struct sockaddr_storage *dst, socklen_t *addrlen) {
  // from https://stackoverflow.com/questions/48328708/, many thanks.
  if (af == AF_INET) {
    struct sockaddr_in *dst_in4 = (struct sockaddr_in *) dst;
    *addrlen = sizeof(*dst_in4);
    memset(dst_in4, 0, *addrlen);
    dst_in4->sin_family = af;
    dst_in4->sin_port = htons(port);
    inet_pton(af, addr, &dst_in4->sin_addr);
  } else {
    struct sockaddr_in6 *dst_in6 = (struct sockaddr_in6 *) dst;
    *addrlen = sizeof(*dst_in6);
    memset(dst_in6, 0, *addrlen);
    dst_in6->sin6_family = af;
    dst_in6->sin6_port = htons(port);
    inet_pton(af, addr, &dst_in6->sin6_addr);
  }
}

int main (int argc, char** argv) {
  fd_set fds;
  char src[INET6_ADDRSTRLEN], dst[INET6_ADDRSTRLEN], ifname[IFNAMSIZ];
  unsigned char *buffer;
  unsigned int tid, ptid, len, mtu = 1500, af = AF_INET, proto = 47;

  if (argc < 2) {
    fprintf(stderr, "Usage: %s [ OPTIONS ] IFNAME { remote RADDR } { local LADDR } { id TID } [ mtu MTU ]\n", argv[0]);
    fprintf(stderr, "where: OPTIONS := { -4 | -6 }\n");
    exit(1);
  }

  strncpy(ifname, argv[1], IFNAMSIZ);

  for(int i = 1; i < argc; i++) {
    if(!strcmp(argv[i], "-4")) strncpy(ifname, argv[++i], IFNAMSIZ);
    if(!strcmp(argv[i], "-6")) {
      strncpy(ifname, argv[++i], INET6_ADDRSTRLEN);
      af = AF_INET6;
      proto = 97;
    }
    if(!strcmp(argv[i], "id")) tid = atoi(argv[++i]);
    if(!strcmp(argv[i], "local")) strncpy(src, argv[++i], INET6_ADDRSTRLEN);
    if(!strcmp(argv[i], "remote")) strncpy(dst, argv[++i], INET6_ADDRSTRLEN);
    if(!strcmp(argv[i], "mtu")) mtu = atoi(argv[++i]);
  }

  int sock_fd = socket(af, SOCK_RAW, proto);

  struct sockaddr_storage laddr, raddr;
  socklen_t laddrlen, raddrlen;
  populate_sockaddr(af, proto, src, &laddr, &laddrlen);
  populate_sockaddr(af, proto, dst, &raddr, &raddrlen);

  if (bind(sock_fd, (struct sockaddr*) &laddr, laddrlen) < 0) {
    fprintf(stderr, "[ERR] can't bind socket: %s\n", strerror(errno));
    exit(1);
  }

  int tap_fd = open("/dev/net/tun", O_RDWR);
  struct ifreq ifr;
  memset(&ifr, 0, sizeof(ifr));
  strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
  ifr.ifr_flags = IFF_TAP | IFF_NO_PI;
  if(ioctl(tap_fd, TUNSETIFF, (void *) &ifr)) {
    fprintf(stderr, "[ERR] can't TUNSETIFF: %s\n", strerror(errno));
    exit(1);
  }

  ifr.ifr_mtu = mtu;

  if (ioctl(socket(AF_INET, SOCK_STREAM, IPPROTO_IP), SIOCSIFMTU, (void *)&ifr) < 0)
    fprintf(stderr, "[WARN] can't SIOCSIFMTU (%s), please set MTU of %s to %d manually.\n", strerror(errno), ifr.ifr_name, ifr.ifr_mtu);

  FD_ZERO(&fds);

  union {
    uint16_t header;
    uint8_t  buffer[65536];
    struct iphdr ip;
    struct eoip_packet eoip;
    struct eoip6_packet eoip6;
  } packet;

  union {
    struct   eoip6_packet eoip6;
    uint16_t header;
  } eoip6_hdr;

  struct eoip_packet eoip_hdr;

  eoip6_hdr.header = htons(EIPHEAD(tid));
  eoip6_hdr.eoip6.head_p1 = BITSWAP(eoip6_hdr.eoip6.head_p1);

  eoip_hdr.tid = tid;
  memcpy(&eoip_hdr.magic, GRE_MAGIC, 4);

  fprintf(stderr, "[INFO] attached to %s, mode %s, remote %s, local %s, tid %d, mtu %d.\n", ifname, af == AF_INET6 ? "EoIPv6" : "EoIP", dst, src, tid, mtu);

  do {
    FD_SET(tap_fd, &fds);
    FD_SET(sock_fd, &fds);

    select(MAX(tap_fd, sock_fd) + 1, &fds, NULL, NULL, NULL);

    if (FD_ISSET(sock_fd, &fds)) {
      len = recv(sock_fd, packet.buffer, sizeof(packet), 0);
      if (af == AF_INET) {
        buffer = packet.buffer;
        buffer += packet.ip.ihl * 4;
        len -= packet.ip.ihl * 4;
        if (memcmp(buffer, GRE_MAGIC, 4)) continue;
        ptid = ((uint16_t *) buffer)[3];
        if (ptid != tid) continue;
        buffer += 8;
        len -= 8;
      } else {
        if(packet.header != eoip6_hdr.header) continue;
        buffer = packet.buffer + 2;
        len -= 2;
      }
      if(len <= 0) continue;
      write(tap_fd, buffer, len);
    }

    if(FD_ISSET(tap_fd, &fds)) {
      if (af == AF_INET) {
        len = read(tap_fd, packet.eoip.payload, sizeof(packet));
        memcpy(packet.eoip.magic, &eoip_hdr, 8);
        packet.eoip.len = htons(len);
        len += 8;
      } else {
        len = read(tap_fd, packet.eoip6.payload, sizeof(packet)) + 2;
        memcpy(&packet.header, &eoip6_hdr.header, 2);
      }
      sendto(sock_fd, packet.buffer, len, 0, (struct sockaddr*) &raddr, raddrlen);
    }
  } while (1);
}

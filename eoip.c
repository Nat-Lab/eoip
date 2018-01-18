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

#define MAX(a,b) (((a)>(b))?(a):(b))
#define GRE_MAGIC "\x20\x01\x64\x00"
#define EIPHEAD(tid) 0x3000 | tid
#define BITSWAP(c) ((c & 0xf0) >> 4) | ((c & 0x0f) << 4);

struct eoip6_packet {
  unsigned char head_p1;
  unsigned char head_p2;
  unsigned char payload[0];
};

struct eoip_packet {
  unsigned char  magic[4];
  unsigned short len;
  unsigned short tid;
  char payload[0];
};

int main (int argc, char** argv) {
  fd_set fds;
  char src[INET6_ADDRSTRLEN], dst[INET6_ADDRSTRLEN], ifname[IFNAMSIZ];
  unsigned char *buffer;
  unsigned int tid, ptid;
  int len, mtu = 1500, af = AF_INET, proto = 47;

  if (argc < 2) {
    fprintf(stderr, "Usage: %s [ OPTIONS ] IFNAME { remote RADDR } { local LADDR } { id TID } [ mtu MTU ]\n", argv[0]);
    fprintf(stderr, "where: OPTIONS := { -4 | -6 }\n");
    exit(1);
  }

  strncpy(ifname, argv[1], INET_ADDRSTRLEN);

  for(int i = 1; i < argc; i++) {
    if(!strcmp(argv[i], "-4")) strncpy(ifname, argv[++i], INET6_ADDRSTRLEN);
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
  if (af == AF_INET) {
    struct sockaddr_in sin;
    sin.sin_port = htons(proto);
    sin.sin_family = af;
    inet_pton(af, src, &sin.sin_addr);
    if(bind(sock_fd, (struct sockaddr*) &sin, sizeof(sin)) < 0) {
      fprintf(stderr, "[ERR] can't bind socket.\n");
      exit(1);
    }
  } else {
    struct sockaddr_in6 sin;
    sin.sin6_port = htons(proto);
    sin.sin6_family = af;
    inet_pton(af, src, &sin.sin6_addr);
    if(bind(sock_fd, (struct sockaddr*) &sin, sizeof(sin)) < 0) {
      fprintf(stderr, "[ERR] can't bind socket.\n");
      exit(1);
    }
  }

  int tap_fd = open("/dev/net/tun", O_RDWR);
  struct ifreq ifr;
  memset(&ifr, 0, sizeof(ifr));
  strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
  ifr.ifr_flags = IFF_TAP | IFF_NO_PI;
  if(ioctl(tap_fd, TUNSETIFF, (void *) &ifr)) {
    fprintf(stderr, "[ERR] can't TUNSETIFF.\n");
    exit(1);
  }

  ifr.ifr_mtu = mtu;

  if (ioctl(socket(AF_INET, SOCK_STREAM, IPPROTO_IP), SIOCSIFMTU, (void *)&ifr) < 0)
    fprintf(stderr, "[WARN] can't SIOCSIFMTU, please set MTU of %s to %d manually.\n", ifr.ifr_name, ifr.ifr_mtu);

  FD_ZERO(&fds);

  union {
    uint16_t header;
    struct iphdr ip;
    struct eoip_packet eoip;
    struct eoip6_packet eoip6;
    unsigned char buffer[65536];
  } packet;

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
        if(memcmp(buffer, GRE_MAGIC, 4)) continue;
        ptid = ((unsigned short *) buffer)[3];
        buffer += 8;
        len -= 8;
      } else {
        packet.eoip6.head_p1 = BITSWAP(packet.eoip6.head_p1);
        ptid = ntohs(packet.header) & 0x0fff;
        buffer = packet.buffer + 2;
        len -= 2;
      }

      if(len <= 0 || tid != ptid) continue;

      write(tap_fd, buffer, len);
    }

    if(FD_ISSET(tap_fd, &fds)) {
      if (af == AF_INET) {
        len = read(tap_fd, packet.eoip.payload, sizeof(packet));
        memcpy(packet.eoip.magic, GRE_MAGIC, 4);
        struct sockaddr_in sin;
        sin.sin_family = af;
        sin.sin_port = htons(proto);
        inet_pton(AF_INET, dst, &sin.sin_addr);
        packet.eoip.len = htons(len);
        packet.eoip.tid = tid;
        sendto(sock_fd, packet.buffer, len + 8, 0, (struct sockaddr*) &sin, sizeof(sin));
      } else {
        len = read(tap_fd, packet.eoip6.payload, sizeof(packet));
        packet.header = htons(EIPHEAD(tid));
        packet.eoip6.head_p1 = BITSWAP(packet.eoip6.head_p1);
        struct sockaddr_in6 sin6;
        sin6.sin6_family = af;
        sin6.sin6_port = htons(proto);
        inet_pton(AF_INET6, dst, &sin6.sin6_addr);
        sendto(sock_fd, packet.buffer, len + 2, 0, (struct sockaddr*) &sin6, sizeof(sin6));
      }
    }
  } while (1);
}

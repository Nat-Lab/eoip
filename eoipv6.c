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

#include <netinet/ip6.h>
#include <arpa/inet.h>

#define MAX(a,b) (((a)>(b))?(a):(b))

struct eoip6_packet {
  unsigned char header[2];
  char payload[0];
};

int main (int argc, char** argv) {

  fd_set fds;
  char src[INET6_ADDRSTRLEN], dst[INET6_ADDRSTRLEN];
  unsigned char *buffer;
  unsigned int tid;
  int len, mtu = 1500;

  if (argc < 2) {
    fprintf(stderr, "Usage: %s IFNAME { src SRC } { dst DST } { tid TID } [ mtu MTU ]\n", argv[0]);
    exit(1);
  }

  for(int i = 2; i < argc; i++) {
    if(!strcmp(argv[i], "tid")) tid = atoi(argv[++i]);
    if(!strcmp(argv[i], "src")) strncpy(src, argv[++i], INET6_ADDRSTRLEN);
    if(!strcmp(argv[i], "dst")) strncpy(dst, argv[++i], INET6_ADDRSTRLEN);
    if(!strcmp(argv[i], "mtu")) mtu = atoi(argv[++i]);
  }

  int sock_fd = socket(AF_INET6, SOCK_RAW, 97);
  struct sockaddr_in6 sin6;
  sin6.sin6_port = htons(97);
  sin6.sin6_family = AF_INET6;
  inet_pton(AF_INET6, src, &sin6.sin6_addr);
  if(bind(sock_fd, (struct sockaddr*) &sin6, sizeof(sin6)) < 0) {
    fprintf(stderr, "[ERR] can't bind socket.\n");
    exit(1);
  }

  int tap_fd = open("/dev/net/tun", O_RDWR);
  struct ifreq ifr;
  memset(&ifr, 0, sizeof(ifr));
  strncpy(ifr.ifr_name, argv[1], IFNAMSIZ);
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
    struct ip6_hdr ip6;
    unsigned char buffer[mtu + 14];
  } packet;

  fprintf(stderr, "[INFO] attached to %s, tid %d.\n", argv[1], tid);

  do {
    FD_SET(tap_fd, &fds);
    FD_SET(sock_fd, &fds);

    select(MAX(tap_fd, sock_fd) + 1, &fds, NULL, NULL, NULL);

    if (FD_ISSET(sock_fd, &fds)) {

      len = recv(sock_fd, packet.buffer, sizeof(packet), 0);

      buffer = packet.buffer;

      fprintf(stderr, "RECV: ");
      for (int i = 0; i < len; i++) fprintf(stderr, "%02hhx ", buffer[i]);
      fprintf(stderr, "\n\n");

      // TODO

      buffer += 2;
      len -= 2;

      if(len <= 0) continue;

      write(tap_fd, buffer, len);
    }

    if(FD_ISSET(tap_fd, &fds)) {

      // TODO

      buffer = packet.buffer;
      len = read(tap_fd, buffer, sizeof(packet)) + 4;

      union {
        struct eoip6_packet eoip;
        unsigned char payload[mtu + 14];
      } buf;

      memcpy(buf.eoip.header, "\x03\x64", 2); // tunnel ID 100
      memcpy(buf.eoip.payload, buffer, len);

      struct sockaddr_in6 sin6;

      sin6.sin6_family = AF_INET6;
      sin6.sin6_port = htons(97);
      inet_pton(AF_INET6, dst, &sin6.sin6_addr);

      fprintf(stderr, "SEND: ");
      for (int i = 0; i < len; i++) fprintf(stderr, "%02hhx ", buf.payload[i]);
      fprintf(stderr, "\n\n");

      fprintf(stderr, "BUFFER: ");
      for (int i = 0; i < len; i++) fprintf(stderr, "%02hhx ", buffer[i]);
      fprintf(stderr, "\n\n");

      sendto(sock_fd, buf.payload, len, 0, (struct sockaddr*) &sin6, sizeof(sin6));
    }

  } while (1);
}

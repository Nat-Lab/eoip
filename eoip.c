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

struct eoip_packet {
  unsigned char  magic[4];
  unsigned short len;
  unsigned short tid;
  char payload[0];
};

int main (int argc, char** argv) {

  fd_set fds;
  char src[INET_ADDRSTRLEN], dst[INET_ADDRSTRLEN];
  unsigned char *buffer;
  unsigned int tid;
  int len, mtu = 1500;

  if (argc < 2) {
    fprintf(stderr, "Usage: %s IFNAME { src SRC } { dst DST } { tid TID } [ mtu MTU ]\n", argv[0]);
    exit(1);
  }

  for(int i = 2; i < argc; i++) {
    if(!strcmp(argv[i], "tid")) tid = atoi(argv[++i]);
    if(!strcmp(argv[i], "src")) strncpy(src, argv[++i], INET_ADDRSTRLEN);
    if(!strcmp(argv[i], "dst")) strncpy(dst, argv[++i], INET_ADDRSTRLEN);
    if(!strcmp(argv[i], "mtu")) mtu = atoi(argv[++i]);
  }

  int sock_fd = socket(AF_INET, SOCK_RAW, 47);
  struct sockaddr_in sin;
  sin.sin_port = htons(47);
  sin.sin_family = AF_INET;
  inet_pton(AF_INET, src, &sin.sin_addr);
  if(bind(sock_fd, (struct sockaddr*) &sin, sizeof(sin)) < 0) {
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
    struct iphdr ip;
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
      buffer += packet.ip.ihl * 4;
      len -= packet.ip.ihl * 4;

      if(memcmp(buffer, GRE_MAGIC, 4)) continue;
      tid = ((unsigned short *) buffer)[1];

      buffer += 8;
      len -= 8;

      if(len <= 0) continue;

      write(tap_fd, buffer, len);
    }

    if(FD_ISSET(tap_fd, &fds)) {

      union {
        struct eoip_packet eoip;
        unsigned char payload[mtu + 14];
      } buf;

      len = read(tap_fd, buf.eoip.payload, sizeof(packet));

      memcpy(buf.eoip.magic, GRE_MAGIC, 4);

      struct sockaddr_in sin;

      sin.sin_family = AF_INET;
      sin.sin_port = htons(47);
      inet_pton(AF_INET, dst, &sin.sin_addr);

      buf.eoip.len = htons(len);
      buf.eoip.tid = tid;

      sendto(sock_fd, buf.payload, len + 8, 0, (struct sockaddr*) &sin, sizeof(sin));
    }

  } while (1);
}

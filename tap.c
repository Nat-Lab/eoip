#include "tap.h"

int make_tap(int *fd, const char *ifname, int mtu) {
  *fd = open("/dev/net/tun", O_RDWR);
  struct ifreq ifr;
  memset(&ifr, 0, sizeof(ifr));
  strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
  ifr.ifr_flags = IFF_TAP | IFF_NO_PI;
  if(ioctl(*fd, TUNSETIFF, (void *) &ifr)) return 1;
  ifr.ifr_mtu = mtu;
  if(ioctl(socket(AF_INET, SOCK_STREAM, IPPROTO_IP), SIOCSIFMTU, (void *)&ifr))
    return 2;
  return 0;
}

void tap_listen(sa_family_t af, int fd, int sock_fd, int tid,
  const struct sockaddr *raddr, socklen_t raddrlen) {
  uint8_t header[8];
  eoip_header(af, tid, &header);
  fd_set fds;
  FD_SET(fd, &fds);
  union packet packet;
  int len;
  do {
    select(fd + 1, &fds, NULL, NULL, NULL);
    if (af == AF_INET) {
      len = read(fd, packet.eoip.payload, sizeof(packet));
      memcpy(&packet.header, &header, 8);
      packet.eoip.len = htons(len);
      len += 8;
    } else {
      len = read(fd, packet.eoip6.payload, sizeof(packet)) + 2;
      memcpy(&packet.header, &header, 2);
    }
    sendto(sock_fd, packet.buffer, len, 0, raddr, raddrlen);
  } while (1);
}

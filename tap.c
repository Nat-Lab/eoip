#include "tap.h"

///
/// make_tap: try to create a TAP interface with given ifname, mtu.
/// return: 0 on success.
///         1 on failed.
///         2 on failed to set MTU.
/// a errno will be set when returned value != 0 (linux)
///
int make_tap(int *fd, const char *ifname, int mtu) {
  #if defined(__APPLE__)
    char devpath[64];
    for(int dev = 0; dev < 16; dev++) {
      sprintf(devpath, "/dev/tap%d", dev);
      if((*fd = open(devpath, O_RDWR))) {
        fprintf(stderr, "[WARN] Running on Darwin, can't set name, the name of interface will be: tap%d\n", dev);
        return 0;
      }
    }
    return 1;
  #elif
    *fd = open(TUNNEL_DEV, O_RDWR);
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
    ifr.ifr_flags = IFF_TAP | IFF_NO_PI;
    if(ioctl(*fd, TUNSETIFF, (void *) &ifr)) return 1;
    ifr.ifr_mtu = mtu;
    if(ioctl(socket(AF_INET, SOCK_STREAM, IPPROTO_IP), SIOCSIFMTU, (void *)&ifr))
      return 2;
  #endif
  return 0;
}

///
/// tap_listen: receive data from a tap w/ given fd and write them to socket
///              w/ given sock_fd and sockaddr.
///
void tap_listen(sa_family_t af, int fd, int sock_fd, int tid,
  const struct sockaddr *raddr, socklen_t raddrlen) {
  uint8_t header[8];
  union packet packet;
  int len;

  // pre-build the header
  eoip_header(af, tid, &header);

  fd_set fds;
  FD_SET(fd, &fds);

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

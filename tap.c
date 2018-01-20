#include "tap.h"

///
/// make_tap: try to create a TAP interface with given ifname, mtu.
/// return: 0 on success.
///         1 on failed.
///         2 on failed to set MTU.
///         3 if we can't set ifname, in this case, read ifname back for name.
/// a errno will be set when returned value != 0 (linux)
///
int make_tap(int *fd, char *ifname, int mtu) {
  struct ifreq ifr;
  memset(&ifr, 0, sizeof(ifr));
  #if defined(__APPLE__) || defined(__OpenBSD__)
    ifr.ifr_flags |= IFF_LINK0;
    char devpath[64];
    for(int dev = 0; dev < TAP_COUNT; dev++) {
      snprintf(devpath, 64, "/dev/tap%d", dev);
      if((*fd = open(devpath, O_RDWR))) {
        ioctl(*fd, SIOCGIFFLAGS, &ifr); // get old flags
        ioctl(*fd, SIOCSIFFLAGS, &ifr); // set IFF_LINK0
        snprintf(ifname, 4, "tap%d", dev);
        return 3;
      }
    }
    return 1;
  #else
    *fd = open(TUNNEL_DEV, O_RDWR);
  #endif
  #if defined(__linux__)
    strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
    ifr.ifr_flags = IFF_TAP | IFF_NO_PI;
    if(ioctl(*fd, TUNSETIFF, (void *) &ifr)) return 1;
    ifr.ifr_mtu = mtu;
    if(ioctl(socket(AF_INET, SOCK_STREAM, IPPROTO_IP), SIOCSIFMTU, (void *)&ifr))
      return 2;
  #elif defined(__FreeBSD__)
    ioctl(*fd, TAPGIFNAME, &ifr);
    strncpy(ifname, ifr.ifr_name, IFNAMSIZ);
    return 3;
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

  do {
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

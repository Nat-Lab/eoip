#ifndef EOIP_TAP_H_
#define EOIP_TAP_H_

#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#if defined(__linux__)
  #include <linux/if_tun.h>
  #define TUNNEL_DEV "/dev/net/tun"
#elif defined(__FreeBSD__)
  #include <net/if_tap.h>
  #define TUNNEL_DEV "/dev/tap"
#elif defined(__APPLE__)
#define TAP_COUNT 16
#elif defined(__OpenBSD__)
#define TAP_COUNT 4
#else
  #error Unsupported platform, will not build.
#endif

#include <arpa/inet.h>
#include "eoip-proto.h"
#include "eoip.h"

int make_tap(int *fd, char *ifname, int mtu);
void tap_listen(sa_family_t af, int fd, int sock_fd, int tid,
  const struct sockaddr *raddr, socklen_t raddrlen);

#endif // EOIP_TAP_H_

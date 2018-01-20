#ifndef EOIP_TAP_H_
#define EOIP_TAP_H_

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/if_tun.h>
#include <arpa/inet.h>
#include "eoip-proto.h"
#include "eoip.h"

int make_tap(int *fd, const char *ifname, int mtu);
void tap_listen(sa_family_t af, int fd, int sock_fd, int tid,
  const struct sockaddr *raddr, socklen_t raddrlen);

#endif // EOIP_TAP_H_

#include "sock.h"

int bind_sock(int *fd, sa_family_t af, int proto, const struct sockaddr *addr,
  const socklen_t addr_len) {
  *fd = socket(af, SOCK_RAW, proto);
  return bind(*fd, addr, addr_len);
}

void sock_listen(sa_family_t af, int fd, int tap_fd, int tid) {
  uint8_t header[8];
  eoip_header(af, tid, &header);
  uint8_t *buffer;

  union packet packet;
  int len;
  fd_set fds;
  FD_SET(fd, &fds);
  do {
    select(fd + 1, &fds, NULL, NULL, NULL);
    len = recv(fd, packet.buffer, sizeof(packet), 0);
    if (af == AF_INET) {
      buffer = packet.buffer;
      buffer += packet.ip.ihl * 4;
      len -= packet.ip.ihl * 4 + 8;
      if (len < 8 || memcmp(buffer, EOIP_MAGIC, 4) || len != ntohs(((uint16_t *) buffer)[2])) continue;
      if (((uint16_t *) buffer)[3] != tid) continue;
      buffer += 8;
    } else {
      if(memcmp(&packet.header, header, 2)) continue;
      buffer = packet.buffer + 2;
      len -= 2;
    }
    if (len <= 0) continue;
    write(tap_fd, buffer, len);
  } while (1);
}

void populate_sockaddr(sa_family_t af, in_port_t port, const char *addr,
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

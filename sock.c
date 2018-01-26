#include "sock.h"

///
/// bind_sock: try to bind() to a sock_fd with given AF, PORT, and sockaddr.
/// return: bind() value
///
int bind_sock(int *fd, sa_family_t af, in_port_t proto,
              const struct sockaddr *addr, const socklen_t addr_len) {
  *fd = socket(af, SOCK_RAW, proto);
  return bind(*fd, addr, addr_len);
}

///
/// sock_listen: receive data from a sock w/ given fd and write them to tap
///              interface w/ given tap_fd.
///
void sock_listen(sa_family_t af, int fd, int tap_fd, int tid,
                 struct sockaddr *raddr, socklen_t raddrlen) {
  uint8_t header[8], *buffer;
  union packet packet;
  int len;
  struct sockaddr_storage saddr;
  socklen_t saddrlen = sizeof(saddr);

  // pre-build the header
  eoip_header(af, tid, &header);

  do {
    len = recvfrom(fd, packet.buffer, sizeof(packet), 0, (struct sockaddr *) &saddr, &saddrlen);

    if (af == AF_INET) {
      // src check
      if(
        ((struct sockaddr_in *) &saddr)->sin_addr.s_addr !=
        ((struct sockaddr_in *) raddr)->sin_addr.s_addr
      ) continue;

      buffer = packet.buffer;

      // skip headres
      buffer += packet.ip.ip_hl * 4;
      len -= packet.ip.ip_hl * 4 + 8;

      // sanity checks
      if (
        len <= 0                               || /* len left < header size */
        memcmp(buffer, EOIP_MAGIC, 4)          || /* not a EOIP packet */
        len != ntohs(((uint16_t *) buffer)[2]) || /* payload len mismatch */
        ((uint16_t *) buffer)[3] != tid           /* tid mismatch */
      ) continue;

      buffer += 8;
    } else {
      // src check
      if (memcmp(
        &((struct sockaddr_in6 *) &saddr)->sin6_addr.s6_addr,
        &((struct sockaddr_in6 *) raddr)->sin6_addr.s6_addr,
        16
      )) continue;

      /* check header, since tid and \x03 are already there, no other checks
         required. */
      if (len < 2 || memcmp(&packet.header, header, 2)) continue;
      buffer = packet.buffer + 2;
      len -= 2;
    }

    if (len <= 0) continue;

    write(tap_fd, buffer, len);
  } while (1);
}

///
/// populate_sockaddr: populate a sockaddr structure with given AF, PORT and
///                    ADDR.
///
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

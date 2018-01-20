#include "eoip-proto.h"
#include "eoip.h"

#include <arpa/inet.h>
#include <string.h>

void populate_eoiphdr(int tid, void *dst) {
  union eoip_hdr *eoip_hdr = (union eoip_hdr *) dst;
  eoip_hdr->eoip.tid = tid;
  memcpy(&eoip_hdr->header, EOIP_MAGIC, 4);
}

void populate_eoip6hdr(int tid, void *dst) {
  union eoip_hdr *eoip6_hdr = (union eoip_hdr *) dst;
  eoip6_hdr->header_v = htons(EIPHEAD(tid));
  eoip6_hdr->eoip6.head_p1 = BITSWAP(eoip6_hdr->eoip6.head_p1);
}

void eoip_header(int af, int tid, void *dst) {
  if (af == AF_INET) populate_eoiphdr(tid, dst);
  if (af == AF_INET6) populate_eoip6hdr(tid, dst);
}

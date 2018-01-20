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
  union eoip6_hdr *eoip6_hdr = (union eoip6_hdr *) dst;
  eoip6_hdr->header = htons(EIPHEAD(tid));
  eoip6_hdr->eoip6.head_p1 = BITSWAP(eoip6_hdr->eoip6.head_p1);
}

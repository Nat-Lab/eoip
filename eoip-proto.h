#ifndef EOIP_PROTO_H_
#define EOIP_PROTO_H_

#include <stdint.h>
#include <arpa/inet.h>
#include <string.h>
#define PROTO_EOIP 47
#define PROTO_EOIP6 97
#define EOIP_MAGIC "\x20\x01\x64\x00"

struct eoip_packet {
  uint8_t  magic[4];
  uint16_t len;
  uint16_t tid;
  uint8_t  payload[0];
};

struct eoip6_packet {
  uint8_t head_p1;
  uint8_t head_p2;
  uint8_t payload[0];
};

union eoip_hdr {
  struct   eoip_packet eoip;
  struct   eoip6_packet eoip6;
  uint8_t  header[8];
  uint16_t header_v;
};

void populate_eoiphdr(int tid, void *dst);
void populate_eoip6hdr(int tid, void *dst);
void eoip_header(int af, int tid, void *dst);

#endif // EOIP_PROTO_H_

eoip
---

This is an implement of MikroTik's [EoIP](http://wiki.mikrotik.com/wiki/Manual:Interface/EoIP)/EoIPv6 tunnel for Linux using TAP. EoIP (Ethernet over IP) and EoIPv6 (Ethernet over IPv6) are MikroTik RouterOS's Layer 2 tunneling protocol.

The EoIP protocol encapsulates Ethernet frames in the GRE (IP protocol number 47) packets, and EoIPv6 protocol encapsulates Ethernet frames in the EtherIP (IP protocol number 97) packets.

### Usage

```
Usage: ./eoip [ OPTIONS ] IFNAME { remote RADDR } { local LADDR } { id TID } [ mtu MTU ]
where: OPTIONS := { -4 | -6 }
```

The parameters are pretty self-explanatory. `RADDR` = remote address, `LADDR` = local address, `TID` = tunnel ID, and `OPTIONS` can be either `-4` (EoIP) or `-6` (EoIPv6), EoIP will be used if unset.

### Example

On MikroTik:

```
[user@mikrotik] > /interface eoip add local-address=172.17.0.1 name=eoip-1 remote-address=172.17.0.2 tunnel-id=100
```

On Linux:

```
# gcc eoip.c -o eoip
# ./eoip -4 tap1 local 172.17.0.2 remote 172.17.0.1 id 100
```

Now `tap1` on Linux is connected to `eoip-1` on MikroTik.

### Protocol

EoIP uses IP Protocol 47, which is the same as GRE.

Here's the packet format of EoIP:

```
0                   1                   2                   3
0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|       GRE FLAGS 0x20 0x01     |      Protocol Type  0x6400    | = "\x20\x01\x64\x00"
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|   Encapsulated frame length   |           Tunnel ID           |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
| Ethernet frame...                                             |
```

(noticed that encapsulated frame length is in BE while Tunnel ID is in LE)

EoIPv6 is much simpler. It use IP Protocol 97, which is the same as EtherIP:

```
0                   1                   2                   3
0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|  EoIP HEADER  | Ethernet frame...                             |
```

Header part of EoIPv6 are similar to [RFC3378](https://tools.ietf.org/html/rfc3378). The 12 reserved bits in the EtherIP header are now Tunnel ID. MikroTik also swaps the first four bits with second four bits in the EtherIP header, so the header looks like this:

```
0   1   2   3   4   5   6   7   8   9  10  11  12  13  14  15
+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
|               |               |                               |
|  TID Part 1   |    VERSION    |          TID Part 2           |
|               |               |                               |
+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
```

where `VERSION` = ``"\x03"``.

### Contribute/Bugs

Feel free to send PR or report issue on the Github project page at:

https://github.com/Nat-Lab/eoip

### Licenses

MIT

#include "eoip.h"

int main (int argc, char** argv) {
  char src[INET6_ADDRSTRLEN], dst[INET6_ADDRSTRLEN], ifname[IFNAMSIZ];
  unsigned int tid = 0, mtu = 1500, daemon = 0;
  sa_family_t af = AF_INET;
  in_port_t proto = PROTO_EOIP;
  uid_t uid = 0;
  gid_t gid = 0;
  pid_t pid = 1;

  if (argc < 2) {
    fprintf(stderr, "Usage: eoip [ OPTIONS ] IFNAME { remote RADDR } { local LADDR } { id TID }\n");
    fprintf(stderr, "                               [ mtu MTU ] [ uid UID ] [ gid GID ] [ fork ]\n");
    fprintf(stderr, "where: OPTIONS := { -4 | -6 }\n");
    exit(1);
  }

  // assume that the first argument is IFNAME
  strncpy(ifname, argv[1], IFNAMSIZ);

  // parse some args
  for(int i = 1; i < argc; i++) {
    if(!strcmp(argv[i], "-4")) strncpy(ifname, argv[++i], IFNAMSIZ);
    if(!strcmp(argv[i], "-6")) {
      strncpy(ifname, argv[++i], INET6_ADDRSTRLEN);
      af = AF_INET6;
      proto = PROTO_EOIP6;
    }
    if(!strcmp(argv[i], "id")) tid = atoi(argv[++i]);
    if(!strcmp(argv[i], "local")) strncpy(src, argv[++i], INET6_ADDRSTRLEN);
    if(!strcmp(argv[i], "remote")) strncpy(dst, argv[++i], INET6_ADDRSTRLEN);
    if(!strcmp(argv[i], "mtu")) mtu = atoi(argv[++i]);
    if(!strcmp(argv[i], "gid")) gid = atoi(argv[++i]);
    if(!strcmp(argv[i], "uid")) uid = atoi(argv[++i]);
    if(!strcmp(argv[i], "fork")) daemon = 1;
  }

  // fork to background?
  if(daemon) {
    pid = fork();
    if(pid < 0) {
      fprintf(stderr, "[ERR] can't daemonize: %s\n", strerror(errno));
      exit(errno);
    }
    if(pid > 0) {
      printf("%d\n", pid);
      exit(0);
    }
  }

  // build sockaddr for send/recv.
  struct sockaddr_storage laddr, raddr;
  socklen_t laddrlen, raddrlen;
  populate_sockaddr(af, proto, src, &laddr, &laddrlen);
  populate_sockaddr(af, proto, dst, &raddr, &raddrlen);

  // bind a sock
  int sock_fd;
  if (bind_sock(&sock_fd, af, proto, (struct sockaddr*) &laddr, laddrlen) < 0) {
    fprintf(stderr, "[ERR] can't bind socket: %s.\n", strerror(errno));
    exit(errno);
  }

  // bind a tap interface
  int tap_fd;
  switch (make_tap(&tap_fd, ifname, mtu)) {
    case 1:
      fprintf(stderr, "[ERR] can't TUNSETIFF: %s.\n", strerror(errno));
      exit(errno);
    case 2:
      fprintf(stderr, "[WARN] can't SIOCSIFMTU (%s), please set MTU manually.\n", strerror(errno));
    default:
      break;
  }

  // change UID/GID?
  if (uid > 0) setuid(uid);
  if (gid > 0) setgid(gid);

  fprintf(stderr, "[INFO] attached to %s, mode %s, remote %s, local %s, tid %d, mtu %d.\n", ifname, af == AF_INET6 ? "EoIPv6" : "EoIP", dst, src, tid, mtu);

  // all set, let's get to work.
  pid_t writer = 1, sender = 1, dead;
  int res, wdead = 0;

  do {
    if (writer == 1) writer = fork();
    if (writer < 0) {
      kill(-1, SIGTERM);
      fprintf(stderr, "[ERR] faild to start TAP listener.\n");
      exit(errno);
    }
    if (writer > 1 && !wdead) sender = fork();
    if (sender < 0) {
      kill(-1, SIGTERM);
      fprintf(stderr, "[ERR] faild to start SOCK listener.\n");
      exit(errno);
    }
    if (wdead) wdead = 0;
    if (writer > 0  && sender > 0) {
      dead = waitpid(-1, &res, 0);
      if (dead == writer) writer = wdead = 1;
      continue;
    }
    prctl(PR_SET_PDEATHSIG, SIGTERM);
    if (!sender) tap_listen(af, tap_fd, sock_fd, tid, (struct sockaddr*) &raddr, raddrlen);
    if (!writer) sock_listen(af, sock_fd, tap_fd, tid);
  } while(1);
}

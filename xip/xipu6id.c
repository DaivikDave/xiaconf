#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <net/xia_fib.h>
#include <net/xia_u6id.h>
#include <xia_socket.h>
#include <asm-generic/errno-base.h>

#include "xip_common.h"
#include "utils.h"
#include "libnetlink.h"
#include "xiart.h"

#define MIN_IPV6_PORT 0
#define MAX_IPV6_PORT 65535

static int usage(void)
{
  fprintf(stderr,
"Usage: xip u6id add UDP_ID [-tunnel [-disable_checksum]]\n"
"       xip u6id del UDP_ID\n"
"       xip u6id show\n"
"where  UDP_ID := HEXDIGIT{20} | IPV6ADDR PORT\n"
"      	IPV6ADDR :=HHHH:HHHH:HHHH:HHHH:HHHH:HHHH:HHHH:HHHH\n"
"       where H := 0-9 | a-f\n"
"       PORT := 0-65535 | \"0x\" 0000-FFFF\n");		  
  return -1;
}

static int modify_local(const struct xia_xid *dst,
						struct local_u6id_info *lu6id_info, int to_add)
{
  struct {
	struct nlmsghdr n;
	struct rtmsg r;
	char buf[1024];
  } req;

  memset(&req, 0, sizeof(req));
  req.n.nlmsg_len = NLMSG_LENGTH(sizeof(struct rtmsg));

  if(to_add) {
	/* XXX Does one really needs all these flags? */
	req.n.nlmsg_flags = NLM_F_REQUEST | NLM_F_CREATE | NLM_F_EXCL;
	req.n.nlmsg_type = RTM_NEWROUTE;
  } else {
	req.n.nlmsg_flags = NLM_F_REQUEST;
	req.n.nlmsg_type = RTM_DELROUTE;
  }

  req.r.rtm_family = AF_XIA;
  req.r.rtm_table = XRTABLE_LOCAL_INDEX;
  req.r.rtm_protocol = RTPROT_BOOT;
  req.r.rtm_type = RTN_LOCAL;
  req.r.rtm_scope = RT_SCOPE_HOST;

  req.r.rtm_dst_len = sizeof(*dst);
  addattr_l(&req.n, sizeof(req), RTA_DST, dst, sizeof(*dst));
  if(to_add)
	addattr_l(&req.n, sizeof(req), RTA_PROTOINFO, lu6id_info, sizeof(*lu6id_info));

  if(rtnl_talk(&rth, &req.n, 0, 0, NULL, NULL, NULL)< 0)
	exit(2);
  return 0;
}

static int do_local(int argc, char **argv, int to_add)
{
  struct xia_xid dst;
  char static_xid[XIA_MAX_STRID_SIZE];
  char *str_xid = &static_xid[0];
  struct local_u6id_info lu6id_info = {
	.tunnel = false;
	.checksum_disabled = false;
  };
  int tunnel = 0;
  int disable_checksum = 0;

  if(!to_add)
	goto parse_id;

  while (argc > 1) {
	char *opt = argv[argc - 1];
	if(matches(opt, "-disable_checksum") == 0 {
		disable_checksum++;
		argc--;
	  } else if(matches(opt, "-tunnel") == 0){
		tunnel++;
		argc--;
	  } else {
		break;
	  }
  }

	if(tunnel > 1 || disable_checksum > 1 || disable_checksum > tunnel) {
	  fprintf(stderr, "Incorrect parameters\n");
	  return usage();
	}

	if(tunnel) {
	  lu6id_info.tunnel = true;
	  lu6id_info.checksum_disabled = disable_checksum;
	}

  parse_id:
	if(argc == 2) {
	  /* Need to convert  IP address and port to an XID. */
	  struct in6_addr ip_addr;
	  long ip_port;
	  int rc;

	  /* Convert IP address string to decimal number. */
	  rc = inet_pton(AF_INET6, argv[0], &ip_addr);
	  if(rc <= 0) {
		fprintf(stderr, "Invalid IPv6 adress\n");
		return usage();
	  } else {
		perror("inet_pton: cannot use IPv6 address");
		exit(1);
	  }

	  /* Convert port string to decimal number. */
	  ip_port = strtol(argv[1], NULL, 0);
	  if(ip_port == LONG_MIN || ip_port == LONG_MAX) {
		perror("strtol: overflow converting port number");
		return usage();
	  }
	  
	  if(ip_port < MIN_IPV6_PORT || ip_port > MAX_IPV6_PORT) {
		fprintf(stderr, "Port must be in range %d - %d\n", MIN_IPV6_PORT, MAX_IPV6_PORT);
		return usage();
	  }
	  int i;
	  for(i=0; i<16; i++) {
		str_xid += sprintf(str_xid, "%02x", ip_addr.s6_addr[i])
	  }
	  sprintf(str_xid,"%04x%08x",(in_port_t)ipport,0);
	  str_xid = static_xid;
	} else if (argc ==1) {
	  /* User has given an XID. */
	  str_xid = argv[0];
	}else {
	  fprintf(stderr, "Wrong number of parameters\n");
	  return usage();
	}
	xrt_get_ppal_id("u6id", usage, &dst, str_xid);
	return modify_local(&dst, &lu6id_info, to_add);
}
static int do_add(int argc, char **argv)
{
  return do_local(argc, argv, 1);
}

static int do_del(int argc, char **argv)
{
  return do_local(argc, argv, 0);
}

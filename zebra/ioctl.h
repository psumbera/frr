// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Common ioctl functions.
 * Copyright (C) 1998 Kunihiro Ishiguro
 */

#ifndef _ZEBRA_IOCTL_H
#define _ZEBRA_IOCTL_H

#ifdef __cplusplus
extern "C" {
#endif

/* Prototypes. */
extern void ifreq_set_name(struct ifreq *ifreq, struct interface *ifp);
extern int if_ioctl(unsigned long request, caddr_t buffer);
extern int vrf_if_ioctl(unsigned long request, caddr_t buffer, vrf_id_t vrf_id);

extern int if_set_flags(struct interface *ifp, uint64_t flags);
extern int if_unset_flags(struct interface *ifp, uint64_t flags);
extern void if_get_flags(struct interface *ifp);

extern void if_get_metric(struct interface *ifp);
extern void if_get_mtu(struct interface *ifp);

#ifdef SOLARIS_IPV6
extern int if_ioctl_ipv6(unsigned long, caddr_t);
extern struct connected *if_lookup_linklocal(struct interface *);

#define AF_IOCTL(af, request, buffer)                                          \
	((af) == AF_INET ? if_ioctl(request, buffer)                           \
			 : if_ioctl_ipv6(request, buffer))
#else  /* SOLARIS_IPV6 */

 #define AF_IOCTL(af, request, buffer)  if_ioctl(request, buffer)

#endif /* SOLARIS_IPV6 */

#ifdef __cplusplus
}
#endif

#endif /* _ZEBRA_IOCTL_H */

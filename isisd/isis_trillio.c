/*
 * IS-IS Rout(e)ing protocol - isis_trillio.c
 *
 * Copyright (C) 2001,2002    Sampo Saaristo
 *                            Tampere University of Technology      
 *                            Institute of Communications Engineering
 *
 * Copyright (c) 2009, 2011, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it 
 * under the terms of the GNU General Public Licenseas published by the Free 
 * Software Foundation; either version 2 of the License, or (at your option) 
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,but WITHOUT 
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or 
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for 
 * more details.

 * You should have received a copy of the GNU General Public License along 
 * with this program; if not, write to the Free Software Foundation, Inc., 
 * 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include <zebra.h>
#include <net/if_dl.h>
#include <sys/socket.h>
#include <sys/stropts.h>
#include <sys/ethernet.h>
#include <net/trill.h>
#include <net/bridge.h>

#include "log.h"
#include "stream.h"
#include "network.h"
#include "if.h"
#include "vty.h"

//#include "isisd/dict.h"
//#include "isisd/include-netbsd/iso.h"
#include "isisd/isis_constants.h"
#include "isisd/isis_common.h"
#include "isisd/isis_circuit.h"
#include "isisd/isis_flags.h"
#include "isisd/isisd.h"
#include "isisd/isis_constants.h"
#include "isisd/isis_circuit.h"
#include "isisd/isis_network.h"
//#include "isisd/isis_tlv.h"
#include "isisd/isis_lsp.h"
//#include "isisd/isis_vlans.h"
#include "isisd/isis_trill.h"

#include "privs.h"

extern struct zebra_privs_t isisd_privs;

static u_char sock_buff[32000];

static const uint8_t all_isis_rbridges[] = ALL_ISIS_RBRIDGES;
static const uint8_t bridge_group_address[] = BRIDGE_GROUP_ADDRESS;

static int
open_trill_socket (struct isis_circuit *circuit)
{
  struct sockaddr_dl laddr;
  int fd;
  unsigned int mtu;

  circuit->fd = -1;

  fd = socket (PF_TRILL, SOCK_DGRAM, 0);
  if (fd < 0)
    {
      zlog_warn ("open_trill_socket(): socket() failed %s",
		 safe_strerror (errno));
      return ISIS_ERROR;
    }

  if (set_nonblocking (fd) < 0)
    {
      zlog_warn ("open_trill_socket(): set_nonblocking() failed: %s",
	  safe_strerror (errno));
      close (fd);
      return ISIS_ERROR;
    }

  if (ioctl (fd, TRILL_NEWBRIDGE, &circuit->area->trill->name) < 0)
    {
      zlog_warn ("open_trill_socket(): TRILL_NEWBRIDGE ioctl failed: %s",
	  safe_strerror (errno));
      close (fd);
      return ISIS_ERROR;
    }

  /*
   * Bind to the physical interface that must be one of the 
   * links in the bridge instance.
   */
  memset (&laddr, 0, sizeof (struct sockaddr_dl));
  laddr.sdl_family = AF_TRILL;
  laddr.sdl_nlen = sizeof (datalink_id_t);
  *(datalink_id_t *)laddr.sdl_data = circuit->interface->ifindex;

  if (bind (fd, (struct sockaddr *) (&laddr), sizeof (struct sockaddr_dl)) < 0)
    {
      zlog_warn ("open_trill_socket(): bind() failed: %s",
	  safe_strerror (errno));
      close (fd);
      return ISIS_ERROR;
    }

  if (ioctl (fd, TRILL_HWADDR, &circuit->u.bc.snpa) < 0)
    {
      zlog_warn ("open_trill_socket(): TRILL_HWADDR ioctl failed: %s",
	  safe_strerror (errno));
      close (fd);
      return ISIS_ERROR;
    }

  if (ioctl (fd, TRILL_GETMTU, &mtu) < 0)
    zlog_warn ("open_trill_socket(): TRILL_GETMTU ioctl failed: %s",
	safe_strerror (errno));
  else
    circuit->interface->mtu = mtu;

  if (mtu > sizeof (sock_buff))
    zlog_err ("open_trill_socket(): interface mtu:%d is greater than "
        " sock_buff size:%d", mtu, sizeof (sock_buff));

  circuit->fd = fd;

  return ISIS_OK;
}

/*
 * Create the socket and set the tx/rx funcs
 */
int
isis_sock_init (struct isis_circuit *circuit)
{
  int retval;

  if (isisd_privs.change (ZPRIVS_RAISE))
    zlog_err ("%s: could not raise privs, %s", __func__, safe_strerror (errno));

  circuit->tx = isis_send_pdu_bcast;
  circuit->rx = isis_recv_pdu_bcast;

  retval = open_trill_socket (circuit);

  if (retval != ISIS_OK)
    {
      zlog_warn ("%s: could not initialize the socket", __func__);
      goto end;
    }

  if (circuit->circ_type == CIRCUIT_T_P2P)
    {
      retval = ISIS_ERROR;
      zlog_err ("%s: do not support P2P link ", __func__);
    }
  else if (circuit->circ_type != CIRCUIT_T_BROADCAST)
    {
      zlog_warn ("%s: unknown circuit type", __func__);
      retval = ISIS_WARNING;
    }

end:
  if (isisd_privs.change (ZPRIVS_LOWER))
    zlog_err ("%s: could not lower privs, %s", __func__, safe_strerror (errno));

  return retval;
}

int
isis_recv_pdu_bcast (struct isis_circuit *circuit, u_char * ssnpa)
{
  int bytesread, addr_len;
  struct sockaddr_dl laddr;
  char *llsaddr;
  uint16_t tci;
  uint8_t sap;

  if (circuit->fd == -1)
    return ISIS_ERROR;

  /* we have to read to the static buff first */
  addr_len = sizeof (struct sockaddr_dl);
  bytesread = recvfrom (circuit->fd, sock_buff, sizeof (sock_buff),
			MSG_DONTWAIT, (struct sockaddr *) &laddr,
			(socklen_t *) &addr_len);

  if (bytesread < 0 && errno == EWOULDBLOCK)
    return ISIS_WARNING;

  if (laddr.sdl_slen != sizeof (tci) || laddr.sdl_alen != ETHERADDRL)
    return ISIS_ERROR;

  if (bytesread < LLC_LEN)
    return ISIS_WARNING;

  llsaddr = LLADDR(&laddr);
  memcpy (ssnpa, llsaddr, laddr.sdl_alen);
  tci = *(uint16_t *)(llsaddr + laddr.sdl_alen);

  sap = tci == TRILL_TCI_BPDU ? ISO_BPDU : ISO_SAP;

  if (sock_buff[0] != sap || sock_buff[1] != sap || sock_buff[2] != 0x03)
    return ISIS_WARNING;

  circuit->vlans->rx_tci = tci;
  stream_write (circuit->rcv_stream, sock_buff + LLC_LEN, bytesread - LLC_LEN);

  return ISIS_OK;
}

int
isis_send_pdu_bcast (struct isis_circuit *circuit, int level)
{
  ssize_t written;
  size_t msglen;
  struct sockaddr_dl laddr;
  char *dp;

  if (circuit->fd == -1)
    return ISIS_ERROR;

  stream_set_getp (circuit->snd_stream, 0);

  laddr.sdl_family = AF_TRILL;
  dp = laddr.sdl_data;

  laddr.sdl_nlen = sizeof (datalink_id_t);
  memcpy (dp, &circuit->interface->ifindex, sizeof (datalink_id_t));
  dp += laddr.sdl_nlen;

  laddr.sdl_alen = ETHERADDRL;
  memcpy (dp, all_isis_rbridges, laddr.sdl_alen);
  dp += laddr.sdl_alen;

  laddr.sdl_slen = sizeof (circuit->vlans->tx_tci);
  memcpy (dp, &circuit->vlans->tx_tci, laddr.sdl_slen);

  /* now set up the data in the buffer */
  sock_buff[0] = ISO_SAP;
  sock_buff[1] = ISO_SAP;
  sock_buff[2] = 0x03;
  msglen = stream_get_endp (circuit->snd_stream);
  if (msglen + LLC_LEN > sizeof (sock_buff))
    return ISIS_WARNING;
  stream_get (sock_buff + LLC_LEN, circuit->snd_stream, msglen);
  msglen += LLC_LEN;

  /* now we can send this */
  written = sendto (circuit->fd, sock_buff, msglen, 0,
		    (struct sockaddr *) &laddr, sizeof (struct sockaddr_dl));

  if (written != (ssize_t)msglen)
    return ISIS_WARNING;

  return ISIS_OK;
}

int
trill_send_bpdu (struct isis_circuit *circuit, const void *msg, size_t msglen)
{
  ssize_t written;
  struct sockaddr_dl laddr;
  char *dp;

  if (circuit->fd == -1)
    return ISIS_ERROR;

  /* add in the LLC header */
  sock_buff[0] = ISO_BPDU;
  sock_buff[1] = ISO_BPDU;
  sock_buff[2] = 0x03;
  memcpy (sock_buff + 3, msg, msglen);
  msglen += 3;

  laddr.sdl_family = AF_TRILL;
  dp = laddr.sdl_data;

  laddr.sdl_nlen = sizeof (datalink_id_t);
  memcpy (dp, &circuit->interface->ifindex, sizeof (datalink_id_t));
  dp += laddr.sdl_nlen;

  laddr.sdl_alen = ETHERADDRL;
  memcpy (dp, bridge_group_address, laddr.sdl_alen);

  laddr.sdl_slen = 0;

  written = sendto (circuit->fd, sock_buff, msglen, 0,
		    (struct sockaddr *) &laddr, sizeof (struct sockaddr_dl));

  if (written != (ssize_t)msglen)
    return ISIS_WARNING;

  return ISIS_OK;
}

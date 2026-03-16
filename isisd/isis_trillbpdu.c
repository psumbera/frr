/*
 * TRILL BPDU handling - isis_trillbpdu.c
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
#include <time.h>
#include "log.h"
#include "if.h"
#include "stream.h"
#include "vty.h"
//#include "dict.h"
#include "isis_common.h"
#include "isis_constants.h"
#include "isis_circuit.h"
//#include "isis_tlv.h"
#include "isis_flags.h"
#include "isis_lsp.h"
#include "isis_trill.h"
#include "isisd.h"
#include "frrevent.h"

/*
 * This module supports just the bare minimum of Bridge PDU handling necessary
 * for normal TRILL interaction with standard bridges.  It does not include
 * spanning tree or other BPDU functions.
 */

struct common_bpdu
{
  uint16_t cmb_protid;		/* Protocol Identifier */
  uint8_t cmb_protvers;	/* Protocol Version Identifier */
  uint8_t cmb_type;		/* BPDU Type */
};

#ifdef __SUNPRO_C
#pragma pack(1)
#endif

struct conf_bpdu
{
  struct common_bpdu cb_cmb;
  uint8_t cb_flags;		/* BPDU Flags */
  uint8_t cb_rootid[8];	/* Root Identifier */
  uint8_t cb_unused[14];	/* Root Path Cost, Bridge ID, Port ID */
  uint16_t cb_messageage;	/* Message Age */
  uint16_t cb_maxage;		/* Max Age */
  uint16_t cb_hello;		/* Hello Time */
  uint16_t cb_unused2;		/* Forward Delay */
} __attribute__ ((packed));

#ifdef __SUNPRO_C
#pragma pack()
#endif

#define	BPDU_PROTID		0	/* Standard STP and RSTP */
#define	BPDU_PROTVERS_STP	0	/* STP */
#define	BPDU_PROTVERS_RSTP	2	/* RSTP */
#define	BPDU_FLAGS_TC_ACK	0x80
#define	BPDU_FLAGS_TC		1
#define	BPDU_TYPE_CONF		0
#define	BPDU_TYPE_RCONF		2
#define	BPDU_TYPE_TCNOTIF	0x80

int
trill_process_bpdu (struct isis_circuit *circuit, u_char *srcaddr)
{
  size_t bpdulen;
  struct conf_bpdu cb;
  time_t now;
  int brcmp;

  /*
   * Standard BPDU validation first.  Unrecognized things are just returned
   * silently.  Bad things (protocol violations) generate warnings.
   */
  bpdulen = stream_get_endp (circuit->rcv_stream);
  if (bpdulen < sizeof (cb.cb_cmb))
    return ISIS_WARNING;

  stream_get (&cb.cb_cmb, circuit->rcv_stream, sizeof (cb.cb_cmb));
  if (ntohs(cb.cb_cmb.cmb_protid) != BPDU_PROTID)
    return ISIS_OK;

  switch (cb.cb_cmb.cmb_type)
  {
    case BPDU_TYPE_CONF:
      if (bpdulen < sizeof (cb))
	return ISIS_WARNING;
      stream_get (&cb.cb_cmb + 1, circuit->rcv_stream,
	  sizeof (cb) - sizeof (cb.cb_cmb));
      if (ntohs(cb.cb_messageage) >= ntohs(cb.cb_maxage))
	return ISIS_WARNING;
      /*
       * We don't send Configuration BPDUs, so no need to check Bridge & Port
       * ID values.
       */
      break;
    case BPDU_TYPE_RCONF:
      if (bpdulen < sizeof (cb) + 1)
	return ISIS_WARNING;
      stream_get (&cb.cb_cmb + 1, circuit->rcv_stream,
	  sizeof (cb) - sizeof (cb.cb_cmb));
      break;
    case BPDU_TYPE_TCNOTIF:
      return ISIS_OK;
    default:
      return ISIS_WARNING;
  }

  brcmp = memcmp (cb.cb_rootid, circuit->root_bridge, sizeof (cb.cb_rootid));
  now = time (NULL);
  if (circuit->root_expire == 0 || now - circuit->root_expire > 0 || brcmp <= 0)
    {
      int hellot;

      hellot = ntohs(cb.cb_hello) / 256;
      if (hellot < 1)
	hellot = 1;
      else if (hellot > 10)
	hellot = 10;
      circuit->root_expire = now + 3 * hellot;
      memcpy(circuit->root_bridge, cb.cb_rootid, sizeof (cb.cb_rootid));

      /* If root bridge change, then inhibit for a while */
      if (brcmp != 0)
	trill_inhib_all (circuit);

     /*
      * If we've gotten a Topology Change Ack from the root bridge, then we
      * need not send any more TC notifications.
      */
      if ((cb.cb_flags & BPDU_FLAGS_TC) && circuit->tc_count != 0)
	{
	  event_cancel(&circuit->tc_thread);
	  circuit->tc_thread = NULL;
	  circuit->tc_count = 0;
	}
    }

  return ISIS_OK;
}

/*
 * Handle TC notification expiry: send another TC BPDU, up to a hard-coded
 * limit.
 */
static void
trill_send_tc (struct event *thread)
{
  struct isis_circuit *circuit;
  struct common_bpdu cmb;
  int retv;

  circuit = EVENT_ARG (thread);

  cmb.cmb_protid = htons (BPDU_PROTID);
  cmb.cmb_protvers = BPDU_PROTVERS_STP;
  cmb.cmb_type = BPDU_TYPE_TCNOTIF;

  retv = trill_send_bpdu (circuit, &cmb, sizeof (cmb));
  if (retv != ISIS_OK)
    zlog_warn ("TRILL unable to send TC BPDU on %s", circuit->interface->name);

  if (++circuit->tc_count <= 5)
    {
      event_add_timer(master, trill_send_tc, circuit, 1, &circuit->tc_thread);
    }
  else
    {
      circuit->tc_thread = NULL;
      circuit->tc_count = 0;
    }

  return;
}

/*
 * Begin sending TC notification BPDUs on this circuit.  Transmissions are sent
 * once a second until either 5 have been sent, or we receive a TC Ack from the
 * root bridge.
 */
void
trill_send_tc_bpdus (struct isis_circuit *circuit)
{
  circuit->tc_count = 1;
  event_add_timer(master, trill_send_tc, circuit, 1, &circuit->tc_thread);
}

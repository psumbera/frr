/*
 * IS-IS Rout(e)ing protocol - isis_trill.h
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

#ifndef _ZEBRA_ISIS_TRILL_H
#define _ZEBRA_ISIS_TRILL_H

#include <stdint.h>
#include <time.h>
#include <net/trill.h>

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRILL_ISIS_LEVEL
#define TRILL_ISIS_LEVEL ISIS_LEVEL2
#endif

#ifndef VTY_NEWLINE
#define VTY_NEWLINE "\n"
#endif

#include "isisd/dict.h"

#ifndef ISIS_MAX_CIRCUITS_COUNT
#define ISIS_MAX_CIRCUITS_COUNT ISIS_MAX_CIRCUITS
#endif

#define	ISO_BPDU	0x42

/* IETF TRILL protocol defined constants */
#define DFLT_NICK_PRIORITY 0x40			/* Default priority for autogen nicks */
#define CONFIGURED_NICK_PRIORITY 0x80		/* MSB of priority set if nick is configured */
#define MIN_RBRIDGE_PRIORITY 1			/* Min priority of use value */
#define MAX_RBRIDGE_PRIORITY 127		/* Max priority of use value */
#define MAX_RBRIDGE_NODES (RBRIDGE_NICKNAME_MAX + 1) /* Max RBridges possible */
#define TRILL_NICKNAME_LEN   2			/* 16-bit nickname */
#define TRILL_DFLT_ROOT_PRIORITY 0x8000		/* Default tree root priority */

#ifndef VLANS_ARRSIZE
#define VLANS_ARRSIZE 512
#endif

/* Constants used in nickname generation/allocation */
#define NICKNAMES_BITARRAY_SIZE (MAX_RBRIDGE_NODES / 8) /* nick usage array */
#define CLEAR_BITARRAY_ENTRYLEN 4	   /* stores nicks available per 32 nicks in nick bitarray */
#define CLEAR_BITARRAY_ENTRYLENBITS (4*8)  /* 32 nicks tracked in each entry */
#define CLEAR_BITARRAY_SIZE (MAX_RBRIDGE_NODES / CLEAR_BITARRAY_ENTRYLENBITS)

/* Constants used to track LSP DB acquisition */
#define MIN_LSPDB_ACQTRIES 2	/* min two LSP PSNP/CSNP send/recv for LSP DB acquisition */
#define MAX_LSPDB_ACQTRIES 6	/* max LSP PSNP/CSNP send/recv for LSP DB acquisition on any circuit */

/* Macros used to track LSP DB acquisition */
#define LSPDB_ACQTRYINC(F, C) ((F)->trill->lspdb_acq_reqs[(C)])++
#define LSPDB_ACQTRYVAL(F, C) ((F)->trill->lspdb_acq_reqs[(C)])

/* trill_info status flags */
#define TRILL_AUTONICK       (1 << 0)  /* nickname auto-generated (else user-provided) */
#define TRILL_LSPDB_ACQUIRED (1 << 1)  /* LSP DB acquired before autogen nick is advertised */
#define TRILL_NICK_SET       (1 << 2)  /* nickname configured (random/user generated) */
#define TRILL_PRIORITY_SET   (1 << 3)  /* nickname priority configured by user */

/* TRILL information (area-specific) */
typedef struct trill_nickname_info
{
  uint8_t priority;
  uint16_t name;
} trill_nickname_info_t;

struct trill_info
{
  trill_nickname_info_t nick;   /* our nick */
  int status; 			/* status flags */
  dict_t *nickdb;	  	/* TRILL nickname database */
  dict_t *sysidtonickdb;  	/* TRILL sysid-to-nickname database */
  /* counter used in LSP database acquisition (per circuit) */
  uint8_t lspdb_acq_reqs [ISIS_MAX_CIRCUITS_COUNT];
  struct list *fwdtbl;          /* RBridge forwarding table */
  struct list *adjnodes;	/* Adjacent nicks for our distrib. tree */
  struct list *dt_roots;	/* Our choice of DT roots */
  struct list *vlans_reachable; /* Per adj and per tree vlans reachable downstream list */
  uint16_t root_priority;       /* Root tree priority */
  char name[MAXLINKNAMELEN];	/* instance name */
};

/* TRILL nickname information (node-specific) */
typedef struct nickinfo
{
  trill_nickname_info_t nick;  /* Nick of the node  */
  u_char sysid[ISIS_SYS_ID_LEN];    /* NET/sysid of node */
  uint8_t flags;                    /* TRILL flags advertised by node */
  struct list *dt_roots;            /* Distrib. Trees chosen by node */
  uint16_t root_priority;           /* Root tree priority */
  uint16_t root_count;		    /* Root tree count */
  struct list *broots;		    /* VLANs and Bridge roots */
  uint8_t vlans_forwarder[VLANS_ARRSIZE];
} nickinfo_t;

/* Nickname database node */
typedef struct trill_nickdb_node
{
  nickinfo_t info;		/* Nick info of the node */
  struct isis_spftree *rdtree;  /* RBridge distribution tree with this nick as root */
  struct list *adjnodes;	/* Our (host RBridge) adjacent nicks on this distrib. tree */
  struct list *vlans_reachable; /* Per adj and per tree vlans reachable downstream list */
  uint32_t refcnt;		/* ref count */
} nicknode_t;

/* RBridge search function return status codes */
typedef enum
{
  NOTFOUND = 1,
  FOUND,
  DUPLICATE,
  NICK_CHANGED,
  PRIORITY_CHANGE_ONLY
} nickdb_search_result;

/* LSP database acquisition process states */
typedef enum
{
  CSNPRCV = 0,
  CSNPSND,
  PSNPSNDTRY,
} lspdbacq_state;

/* RBridge forwarding table node (1 table per area) */
typedef struct nickfwdtable_node
{
  uint16_t dest_nick;                /* destination RBridge nick */
  u_char adj_snpa[ETH_ALEN];         /* MAC address of the adj node */
  struct interface *interface;       /* if to reach the adj/neigh */
} nickfwdtblnode_t;

struct trill_circuit_vlans {
  uint16_t pvid;
  uint16_t designated;
  uint16_t our_designated;
  uint16_t tx_tci;
  uint16_t rx_tci;
  time_t inhibit_all;
  uint8_t enabled[VLANS_ARRSIZE];
  uint8_t forwarder[VLANS_ARRSIZE];
  struct list *enabled_vlans;
  struct list *appvlanfwders;
  struct list *inhibit_vlans;
  struct event *inhibit_thread;
  uint32_t vlanfwdershash;
};

/* Per-adjacency TRILL VLAN state. */
struct trill_adj_vlans {
  uint16_t designated;
  uint8_t enabled[VLANS_ARRSIZE];
  uint8_t forwarder[VLANS_ARRSIZE];
  uint8_t seen[VLANS_ARRSIZE];
};

struct trill_inhibit_vlan {
  uint16_t vlan;
  time_t reenable;
};

void trill_read_config (char **, int, char **);
void trill_area_init(struct isis_area *);
void trill_area_free(struct isis_area *);
void trill_get_area_nickinfo(struct isis_area *, trill_nickname_info_t *);
void trill_nickdb_print (struct vty *, struct isis_area *);
void trill_nick_destroy(struct isis_lsp *);
void trill_lspdb_acquire_event(struct isis_circuit *, lspdbacq_state);
int trill_area_nickname(struct isis_area *, uint16_t);
void trill_parse_router_capability_tlvs (struct isis_area *, struct isis_lsp *);
void trill_process_spf (struct isis_area *);
void trill_process_hello(struct isis_adjacency *, struct list *);
void send_trill_vlan_hellos(struct isis_circuit *);
void trill_circuits_print_all (struct vty *, struct isis_area *);
u_char *nick_to_sysid(struct isis_area *, uint16_t);
uint16_t sysid_to_nick(struct isis_area *, u_char *);
void trill_create_vlanfilterlist(struct isis_area *, nicknode_t *);
nicknode_t * trill_nicknode_lookup(struct isis_area *, uint16_t);
void install_trill_elements (void);
void install_trill_vlan_elements (void);
int trill_process_bpdu (struct isis_circuit *, u_char *);
int trill_send_bpdu (struct isis_circuit *circuit, const void *, size_t);
void trill_send_tc_bpdus (struct isis_circuit *);
void trill_set_vlan_forwarder (struct isis_circuit *, uint8_t *);
void trill_port_flush (struct isis_circuit *, uint16_t);
void trill_nick_flush (struct isis_circuit *, uint16_t);
void trill_inhib_all(struct isis_circuit *);
char trill_reload(void);
#endif

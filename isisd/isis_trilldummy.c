/*
 * IS-IS Rout(e)ing protocol - isis_trilldummy.c
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
#include <vty.h>
#include <if.h>

#include "isisd/dict.h"
#include <stdbool.h>
#include "isisd/isis_constants.h"
#include "isisd/isis_common.h"
#include "isisd/isis_flags.h"
#include "isisd/isisd.h"
#include "isisd/isis_adjacency.h"
#include "isisd/isis_circuit.h"
#include "isisd/isis_tlvs.h"
#include "isisd/isis_lsp.h"
#include "isisd/isis_trill.h"

void trill_read_config (char **cfilep, int argc, char **argv) { }
void trill_process_hello(struct isis_adjacency *adj, struct list *portcaps) { }
void trill_nickdb_print (struct vty *vty, struct isis_area *area) { }
void trill_lspdb_acquire_event(struct isis_circuit *circuit,
    lspdbacq_state caller) { }
void trill_nick_destroy(struct isis_lsp *lsp) { }
void send_trill_vlan_hellos(struct isis_circuit *circuit) { }
void trill_area_init(struct isis_area *area) { }
void trill_area_free(struct isis_area *area) { }
void trill_parse_router_capability_tlvs (struct isis_area *area,
    struct isis_lsp *lsp) { }
void trill_process_spf (struct isis_area *area) { }
int tlv_add_trill_nickname(trill_nickname_info_t *nick_info,
    struct stream *stream, struct isis_area *area) { return ISIS_OK; }
int tlv_add_trill_vlans(struct isis_circuit *circuit) { return ISIS_OK; }
void install_trill_elements (void) { }
void install_trill_vlan_elements (void) { }
int trill_process_bpdu (struct isis_circuit *c, u_char *sa) { return ISIS_OK; }
char trill_reload(void) { return 0; }

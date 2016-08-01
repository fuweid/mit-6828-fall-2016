#include "ns.h"
#include <inc/lib.h>
#include <kern/e1000.h>
extern union Nsipc nsipcbuf;

void
output(envid_t ns_envid)
{
	binaryname = "ns_output";

	// LAB 6: Your code here:
	// 	- read a packet from the network server
	//	- send the packet to the device driver
   //
   int r;
   envid_t envid;
   struct tx_desc td;

   while(1) {
     envid = 0;
     r = ipc_recv(&envid, &nsipcbuf, 0);

     if ((envid != ns_envid) ||
         (r != NSREQ_OUTPUT)) {
        continue;
     }

     if (thisenv->env_ipc_value == NSREQ_OUTPUT) {

       memset(&td, 0, sizeof(struct tx_desc));
       td.addr = (uint32_t) nsipcbuf.pkt.jp_data;
       td.length = (uint16_t) nsipcbuf.pkt.jp_len;
       //td.cmd |= E1000_TXD_CMD_EOP;
       //td.cmd |= E1000_TXD_CMD_RS;
       td.cmd = 9;

       sys_net_push_tx_desc(&td);
     }
   }
}

#include "ns.h"
#include <inc/lib.h>
#include <kern/e1000.h>
extern union Nsipc nsipcbuf;

void
input(envid_t ns_envid)
{
	binaryname = "ns_input";

	// LAB 6: Your code here:
	// 	- read a packet from the device driver
	//	- send it to the network server
	// Hint: When you IPC a page to the network server, it will be
	// reading from it for a while, so don't immediately receive
	// another packet in to the same physical page.
   int r;

   while(1) {
     if ((r = sys_page_alloc(0, &nsipcbuf, PTE_U| PTE_W| PTE_P)) < 0)
       panic("input: %e", r);

     struct rx_desc rd = {0, 0, 0, 0, 0, 0};
     rd.addr = (uint32_t) nsipcbuf.pkt.jp_data;
     while ((r = sys_net_receive_rx_desc(&rd)) != 0) {
       if (r == -E_RQ_EMPTY) {
         sys_yield(); // will pause whole process - -!
         continue;
       } else {
         panic("Oops...");
       }
     }

     nsipcbuf.pkt.jp_len = rd.length;
     ipc_send(ns_envid, NSREQ_INPUT, &nsipcbuf, PTE_U | PTE_W | PTE_P);
     sys_page_unmap(0, &nsipcbuf);
   }
}

#include <kern/e1000.h>

// LAB 6: Your driver code here
#include <inc/x86.h>
#include <inc/assert.h>
#include <inc/error.h>
#include <inc/string.h>
#include <kern/pmap.h>

volatile uint32_t *e1000_io_base;
volatile uint32_t *e1000_tdt;
volatile uint32_t *e1000_rxt;

int
e1000_attach(struct pci_func *pcif)
{
  pci_func_enable(pcif);

  e1000_io_base = mmio_map_region(pcif->reg_base[0], pcif->reg_size[0]);

  if (read_e1000_register(E1000_STATUS) == 0x80080783)
    cprintf("E1000: A full duplex link is up at 1000 MB/s\n");

  e1000_transmit_init();

  e1000_receive_init();
  //check_e1000_transmit();
  //check_e1000_receive();
  return 0;
}

void
e1000_transmit_init()
{
  int i;
  uint8_t *addr;
  uint32_t flag;
  //step 1. Allocate a region of memory for the transmit descriptor list.
  memset(tx_descs, 0, sizeof(tx_descs));
  for (i = 0; i < NTXDESCS; i++)
    tx_descs[i].status = E1000_TXD_STAT_DD;

  addr = (uint8_t *) e1000_io_base + E1000_TDBAL;
  *((uint32_t *) addr) = PADDR(tx_descs);
  addr = (uint8_t *) e1000_io_base + E1000_TDBAH;
  *((uint32_t *) addr) = 0;

  //step 2. Set the Transmit Descriptor Length (TDLEN) register to the size (in bytes)
  //of the descriptor ring.
  addr = (uint8_t *) e1000_io_base + E1000_TDLEN;
  *((uint32_t *) addr) = sizeof(tx_descs);

  //step 3. The Transmit Descriptor Head and Tail (TDH/TDT) registers are
  //initialized (by hardware) to 0b.
  addr = (uint8_t *) e1000_io_base + E1000_TDH;
  *((uint32_t *) addr) = 0;
  addr = (uint8_t *) e1000_io_base + E1000_TDT;
  *((uint32_t *) addr) = 0;
  e1000_tdt = (uint32_t *)addr;

  //step 4. Initialize the Transmit Control Register (TCTL)
  // Table 13-76. TCTL Register Bit Description
  flag = 0;
  flag |= E1000_TCTL_EN;
  flag |= E1000_TCTL_PSP;
  flag |= (0x10 << 4);
  flag |= (0x40) << 12;
  addr = (uint8_t *) e1000_io_base + E1000_TCTL;
  *((uint32_t *) addr) = flag;

  //step 5. Program the Transmit IPG (TIPG) register
  //Table 13-77. TIPG Register Bit Description
  flag = 0;
  flag |= 10;
  flag |= 4 << 10;
  flag |= 6 << 20;
  addr = (uint8_t *) e1000_io_base + E1000_TIPG;
  *((uint32_t *) addr) = flag;
}

void
e1000_receive_init()
{
  // skip multicast mode and long packets
  //
  // skip interrput
  //
  //
  int i;
  uint8_t *addr;
  uint32_t flag;
  struct PageInfo *pp;


  addr = (uint8_t *)e1000_io_base + E1000_RA;
  *((uint32_t *) addr) = 0x12005452;
  addr = addr + sizeof(uint32_t);
  *((uint32_t *)addr) = 0x5634 | E1000_RAH_AV;

  // DMA
  memset(rx_descs, 0, sizeof(rx_descs));
  for (i = 0; i < NRXDESCS; i++) {
    pp = page_alloc(0);
    if (!pp)
      panic("no more space");
    rx_descs[i].addr = page2pa(page_alloc(0));
  }

  // Allocate a region of memory for the receive descriptor list.
  // Software should insure this memory is aligned on a paragraph (16-byte)
  // boundary. Program the Receive Descriptor Base Address (RDBAL/RDBAH)
  // register(s) with the address of the region. RDBAL is used for 32-bit
  // addresses and both RDBAL and RDBAH are used for 64-bit addresses.
  addr = (uint8_t *) e1000_io_base + E1000_RDBAL;
  *((uint32_t *) addr) = PADDR(rx_descs);
  addr = (uint8_t *) e1000_io_base + E1000_RDBAH;
  *((uint32_t *) addr) = 0;

  // Set the Receive Descriptor Length (RDLEN) register to the size
  // (in bytes) of the descriptor ring. This register must be 128-byte aligned.
  addr = (uint8_t *) e1000_io_base + E1000_RDLEN;
  *((uint32_t *) addr) = sizeof(rx_descs);

  //The Receive Descriptor Head and Tail registers are initialized (by hardware)
  //to 0b after a power-on or a software-initiated Ethernet controller reset.
  //Receive buffers of appropriate size should be allocated and pointers to
  //these buffers should be stored in the receive descriptor ring.
  //Software initializes the Receive Descriptor Head (RDH) register and Receive
  //Descriptor Tail (RDT) with the appropriate head and tail addresses.
  //Head should point to the first valid receive descriptor in the descriptor
  //ring and tail should point to one descriptor beyond the last valid
  //descriptor in the descriptor ring.
  addr = (uint8_t *) e1000_io_base + E1000_RDH;
  *((uint32_t *) addr) = 0;
  addr = (uint8_t *) e1000_io_base + E1000_RDT;
  *((uint32_t *) addr) = NRXDESCS - 1;
  e1000_rxt = (uint32_t *)addr;

  flag = 0;
  flag |= E1000_RCTL_EN;
  //flag |= E1000_RCTL_LPE;
  flag &= (~E1000_RCTL_DTYP_MASK);
  flag |= E1000_RCTL_BAM;
  flag |= E1000_RCTL_SZ_2048;
  flag |= E1000_RCTL_SECRC;
  addr = (uint8_t *) e1000_io_base + E1000_RCTL;
  *((uint32_t *) addr) = flag;
}

void
check_e1000_transmit()
{
  char msg[] = "Hello World!";
  struct tx_desc td;
  int i, r, c = 0;

  memset(&td, 0, sizeof(struct tx_desc));
  td.addr = PADDR(msg);
  td.length = sizeof(msg);
  td.cmd |= 9;

  for (i = 0; i < 10; i++) {
    if ((r = e1000_push_tx_desc(&td)) < 0)
      cprintf("Transmit Descriptor Queue is Full\n");
  }
}

void
check_e1000_receive()
{
  struct rx_desc rd;

  cprintf("hello\n");
  while (e1000_receive_rx_desc(&rd) == 0) {
    cprintf("%.s", rd.addr, rd.length);
  }
}

uint32_t
e1000_push_tx_desc(struct tx_desc *td)
{
  struct tx_desc *txd = &tx_descs[*e1000_tdt];
  if (!(txd->status & E1000_TXD_STAT_DD))
    return -E_TQ_FULL;

  memset(txd, 0, sizeof(struct tx_desc));
  *txd = *td;
  txd->cmd |= 8;
  *e1000_tdt = ((*e1000_tdt) + 1) % NTXDESCS;
  return 0;
}

uint32_t
e1000_receive_rx_desc(struct rx_desc *rd)
{
  struct rx_desc *rxd = &rx_descs[(*e1000_rxt + 1) % NRXDESCS];
  if (rxd->status & E1000_RXD_STAT_DD) {
    if (!(rxd->status & E1000_RXD_STAT_EOP))
      panic("Oops! We don't support multi-packets");

    memset(rd, 0, sizeof(struct rx_desc));
    *rd = *rxd;
    rxd->status = 0;
    *e1000_rxt = (*e1000_rxt + 1) % NRXDESCS;
    return 0;
  } else {
    return -E_RQ_EMPTY;
  }
}

uint32_t
read_e1000_register(uint32_t offset)
{
  return *(uint32_t *)((uint8_t *)e1000_io_base + offset);
}

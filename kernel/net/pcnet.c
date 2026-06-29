#include "kernel.h"
#include "net/pci.h"
#include "net/net.h"
#include "net/eth.h"
#include "net/pcnet.h"

#define PCNET_VENDOR  0x1022u
#define PCNET_DEVICE  0x2000u

/* I/O register offsets (32-bit mode) */
#define R_APROM  0x00u
#define R_RDP    0x10u   /* CSR data port  */
#define R_RAP    0x14u   /* CSR addr port  */
#define R_RESET  0x18u   /* sw reset (read)*/
#define R_BDP    0x1Cu   /* BCR data port  */

/* ── CSR0 bit masks ──────────────────────────────────────────
   RC = read-clear (write 1 to clear, write 0 = no change)
   RW = read-write (write 0 clears the bit)                 */
#define CSR0_INIT  0x0001u   /* W1: start init sequence        */
#define CSR0_STRT  0x0002u   /* RW: keep chip running          */
#define CSR0_STOP  0x0004u   /* RW: stop chip                  */
#define CSR0_TDMD  0x0008u   /* W1: demand TX poll (auto-clr)  */
#define CSR0_INEA  0x0040u   /* RW: interrupt enable (unused)  */
#define CSR0_IDON  0x0100u   /* RC: init done                  */
#define CSR0_TINT  0x0200u   /* RC: TX complete                */
#define CSR0_RINT  0x0400u   /* RC: RX complete                */

/* Descriptor status bits (in stat / upper-16 of RMD1) */
#define DS_OWN   0x8000u   /* 1 = NIC owns, 0 = host owns */
#define DS_STP   0x0200u   /* Start-of-packet             */
#define DS_ENP   0x0100u   /* End-of-packet               */
#define DS_ERR   0x4000u   /* Error summary               */

#define RX_LOG2  3              /* 2^3 = 8 RX descriptors (was 4, more breathing room) */
#define TX_LOG2  2
#define RX_CNT   (1 << RX_LOG2)
#define TX_CNT   (1 << TX_LOG2)
#define BUF_SZ   1536u

/* 16-byte PCnet descriptor (SWSTYLE=2) */
typedef struct __attribute__((packed, aligned(16))) {
    uint32_t addr;
    int16_t  bcnt;   /* Two's complement of buf size; bits[15:12] must be 1 */
    uint16_t stat;
    uint32_t mcnt;   /* [11:0] = received byte count */
    uint32_t res;
} PCnetDesc;

/* 28-byte init block (SWSTYLE=2) */
typedef struct __attribute__((packed, aligned(4))) {
    uint16_t mode;
    uint8_t  rlen;       /* log2(rx_cnt) << 4 */
    uint8_t  tlen;       /* log2(tx_cnt) << 4 */
    uint8_t  padr[6];
    uint16_t res;
    uint32_t ladrf[2];   /* multicast filter (0 = unicast+broadcast only) */
    uint32_t rdra;
    uint32_t tdra;
} PCnetInit;

/* DMA-safe static buffers (identity-mapped in VOS: VA == PA) */
static PCnetDesc _rx_ring[RX_CNT] __attribute__((aligned(16)));
static PCnetDesc _tx_ring[TX_CNT] __attribute__((aligned(16)));
static uint8_t   _rx_buf[RX_CNT][BUF_SZ] __attribute__((aligned(16)));
static uint8_t   _tx_buf[TX_CNT][BUF_SZ] __attribute__((aligned(16)));
static PCnetInit _init                    __attribute__((aligned(4)));

uint16_t pcnet_io = 0;
static int _rx_head = 0;
static int _tx_head = 0;

/* ── Register helpers ──────────────────────────────────────── */
static uint32_t _rcsr(uint32_t n)
    { outl(pcnet_io + R_RAP, n); return inl(pcnet_io + R_RDP); }
static void _wcsr(uint32_t n, uint32_t v)
    { outl(pcnet_io + R_RAP, n); outl(pcnet_io + R_RDP, v); }
static void _wbcr(uint32_t n, uint32_t v)
    { outl(pcnet_io + R_RAP, n); outl(pcnet_io + R_BDP, v); }

/* ── Init ──────────────────────────────────────────────────── */
int pcnet_init(void) {
    uint8_t bus, slot, func;
    if (!pci_find(PCNET_VENDOR, PCNET_DEVICE, &bus, &slot, &func)) return 0;

    uint32_t bar0 = pci_bar(bus, slot, func, 0);
    if (!(bar0 & 1)) return 0;
    pcnet_io = (uint16_t)(bar0 & 0xFFFCu);

    pci_enable_busmaster(bus, slot, func);

    /* ── 1. 32-bit software reset ──────────────────────────── */
    /* A DWORD read of R_RESET resets chip and switches to 32-bit I/O mode.
       After reset: STOP=1, STRT=0, SWSTYLE=0 (default).               */
    inl(pcnet_io + R_RESET);
    for (volatile int i = 0; i < 100000; i++);   /* ≥1 µs required */

    /* Explicit STOP — guarantees chip is halted before re-init */
    _wcsr(0, CSR0_STOP);

    /* ── 2. Switch to SWSTYLE=2 (32-bit descriptors) ─────── */
    /* BCR20: SSIZE32(bit8)=1, SWSTYLE(bits1:0)=2             */
    _wbcr(20, 0x0102u);

    /* ── 3. Read MAC from APROM ─────────────────────────────  */
    for (int i = 0; i < 6; i++)
        net_mac.b[i] = inb(pcnet_io + R_APROM + (uint16_t)i);

    /* ── 4. CSR4: standard PCnet-32 setup ───────────────────
       APAD_XMT(11): auto-pad TX to 64 bytes             ON
       MFCOM(8):     mask missed-frame-counter interrupt  ON
       RCVCCOM(4):   mask RX-collision interrupt          ON
       TXSTRTM(2):   mask TX-start interrupt              ON
       JABM(0):      mask jabber interrupt                ON  */
    _wcsr(4, 0x0915u);

    /* ── 5. CSR3: mask all interrupts (polling mode) ──────── */
    _wcsr(3, 0x5F00u);   /* mask BABL, MERR, IDON, TINT, RINT, MISS errors */

    /* ── 6. Init RX ring ─────────────────────────────────── */
    for (int i = 0; i < RX_CNT; i++) {
        _rx_ring[i].addr = (uint32_t)(uint64_t)_rx_buf[i];
        /* bcnt: two's complement, bits[15:12] must be 1 (auto with -BUF_SZ) */
        _rx_ring[i].bcnt = -(int16_t)BUF_SZ;
        _rx_ring[i].stat = DS_OWN;   /* give to NIC */
        _rx_ring[i].mcnt = 0;
        _rx_ring[i].res  = 0;
    }

    /* ── 7. Init TX ring ─────────────────────────────────── */
    for (int i = 0; i < TX_CNT; i++) {
        _tx_ring[i].addr = (uint32_t)(uint64_t)_tx_buf[i];
        _tx_ring[i].bcnt = 0;
        _tx_ring[i].stat = 0;   /* host owns */
        _tx_ring[i].mcnt = 0;
        _tx_ring[i].res  = 0;
    }

    /* ── 8. Build and load init block ───────────────────────  */
    _init.mode     = 0x0000u;   /* normal mode, no loopback */
    _init.rlen     = (uint8_t)(RX_LOG2 << 4);
    _init.tlen     = (uint8_t)(TX_LOG2 << 4);
    for (int i = 0; i < 6; i++) _init.padr[i] = net_mac.b[i];
    _init.res      = 0;
    _init.ladrf[0] = 0;
    _init.ladrf[1] = 0;
    _init.rdra     = (uint32_t)(uint64_t)_rx_ring;
    _init.tdra     = (uint32_t)(uint64_t)_tx_ring;

    uint32_t iba = (uint32_t)(uint64_t)&_init;
    _wcsr(1, iba & 0xFFFFu);
    _wcsr(2, iba >> 16);

    /* ── 9. INIT sequence ───────────────────────────────────  */
    _wcsr(0, CSR0_INIT);

    /* Wait for IDON — chip reads init block and programs itself */
    int ok = 0;
    for (int i = 0; i < 200000; i++) {
        if (_rcsr(0) & CSR0_IDON) { ok = 1; break; }
    }
    if (!ok) { pcnet_io = 0; return 0; }   /* init failed */

    /* ── 10. Start chip: clear IDON, assert STRT ────────────  */
    _wcsr(0, CSR0_STRT | CSR0_IDON);   /* STRT=1, IDON=1(clear) */

    return 1;
}

/* ── Send ──────────────────────────────────────────────────── */
int pcnet_send(const uint8_t *data, uint16_t len) {
    if (!pcnet_io || len > BUF_SZ) return -1;

    PCnetDesc *td = &_tx_ring[_tx_head];

    /* Wait until host owns descriptor (NIC finished previous TX) */
    for (int i = 0; i < 50000 && (td->stat & DS_OWN); i++);
    if (td->stat & DS_OWN) return -1;

    /* Copy frame into DMA-safe TX buffer */
    for (uint16_t i = 0; i < len; i++) _tx_buf[_tx_head][i] = data[i];

    /* Set up descriptor */
    td->bcnt = -(int16_t)len;
    td->mcnt = 0;
    td->res  = 0;
    /* Compiler barrier: ensure all writes above are visible before OWN */
    __asm__ volatile("" ::: "memory");
    td->stat = (uint16_t)(DS_OWN | DS_STP | DS_ENP);

    /* Demand TX: write STRT|TDMD — keeps chip running, triggers ring poll.
       Do NOT use _rcsr(0)|TDMD — that read clears RC bits unnecessarily.  */
    _wcsr(0, CSR0_STRT | CSR0_TDMD);

    /* Wait for NIC to take the descriptor (confirms actual transmission) */
    for (int i = 0; i < 200000 && (td->stat & DS_OWN); i++);

    /* Clear TINT without stopping chip */
    _wcsr(0, CSR0_STRT | CSR0_TINT);

    _tx_head = (_tx_head + 1) % TX_CNT;
    return (td->stat & DS_OWN) ? -1 : 0;
}

/* ── Poll RX ring ──────────────────────────────────────────── */
void pcnet_poll(void) {
    for (;;) {
        volatile PCnetDesc *rd = (volatile PCnetDesc *)&_rx_ring[_rx_head];

        /* Compiler + CPU barrier: re-read stat from memory every iteration */
        __asm__ volatile("" ::: "memory");
        if (rd->stat & DS_OWN) break;   /* NIC still owns → no packet yet */

        /* Only process complete, error-free frames (STP+ENP set, ERR clear) */
        if ((rd->stat & (DS_STP | DS_ENP | DS_ERR)) == (DS_STP | DS_ENP)) {
            uint16_t len = (uint16_t)(rd->mcnt & 0x0FFFu);
            if (len > 4)
                eth_receive(_rx_buf[_rx_head], (uint16_t)(len - 4u));
        }

        /* Return descriptor to NIC */
        rd->mcnt = 0;
        rd->res  = 0;
        rd->bcnt = -(int16_t)BUF_SZ;
        __asm__ volatile("" ::: "memory");   /* barrier before giving OWN */
        rd->stat = DS_OWN;

        _rx_head = (_rx_head + 1) % RX_CNT;
    }

    /* Clear RINT without stopping chip */
    _wcsr(0, CSR0_STRT | CSR0_RINT);
}
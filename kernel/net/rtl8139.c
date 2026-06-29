#include "kernel.h"
#include "net/pci.h"
#include "net/net.h"
#include "net/rtl8139.h"
#include "net/eth.h"

/* ── RTL8139 PCI identifiers ─────────────────────────────── */
#define RTL_VENDOR  0x10EC
#define RTL_DEVICE  0x8139

/* ── Register offsets (relative to IO base) ──────────────── */
#define R_MAC0      0x00
#define R_TSD0      0x10   /* TX Status Descriptors 0-3 (+4 each) */
#define R_TSAD0     0x20   /* TX Start Addresses    0-3 (+4 each) */
#define R_RBSTART   0x30   /* RX ring buffer start (phys addr) */
#define R_CR        0x37   /* Command Register */
#define R_CAPR      0x38   /* Current Address of Packet Read */
#define R_CBR       0x3A   /* Current Buffer Address */
#define R_IMR       0x3C   /* Interrupt Mask Register */
#define R_ISR       0x3E   /* Interrupt Status Register */
#define R_TCR       0x40   /* TX Config */
#define R_RCR       0x44   /* RX Config */
#define R_CONFIG1   0x52

/* ── CR bits ─────────────────────────────────────────────── */
#define CR_RST      0x10
#define CR_RE       0x08
#define CR_TE       0x04
#define CR_BUFE     0x01   /* RX buffer empty */

/* ── ISR bits ────────────────────────────────────────────── */
#define ISR_TOK     0x0004
#define ISR_ROK     0x0001

/* ── RX buffer: 8 KB ring + 16-byte pad + 1500 overflow ─── */
#define RX_BUF_LEN  8192
#define RX_BUF_PAD  16
#define RX_BUF_OVF  1500
#define RX_BUF_SIZE (RX_BUF_LEN + RX_BUF_PAD + RX_BUF_OVF)

#define TX_BUF_SIZE 1536
#define TX_DESC_CNT 4

/* ── Static buffers (must have physical addr == virtual addr) */
static uint8_t  _rx[RX_BUF_SIZE] __attribute__((aligned(4)));
static uint8_t  _tx[TX_DESC_CNT][TX_BUF_SIZE] __attribute__((aligned(4)));

/* ── Driver state ────────────────────────────────────────── */
uint16_t rtl8139_io = 0;
static int      _tx_cur = 0;
static uint16_t _rx_ptr = 0;

/* ── Init ──────────────────────────────────────────────────── */
int rtl8139_init(void) {
    uint8_t bus, slot, func;
    if (!pci_find(RTL_VENDOR, RTL_DEVICE, &bus, &slot, &func)) return 0;

    pci_enable_busmaster(bus, slot, func);

    /* BAR0 = I/O port base (bit 0 = I/O indicator) */
    rtl8139_io = (uint16_t)(pci_bar(bus, slot, func, 0) & 0xFFFC);

    /* Wake from power-save */
    outb(rtl8139_io + R_CONFIG1, 0x00);

    /* Software reset; poll until bit clears */
    outb(rtl8139_io + R_CR, CR_RST);
    { int t = 100000; while ((inb(rtl8139_io + R_CR) & CR_RST) && t-- > 0); }

    /* RX ring buffer physical address */
    outl(rtl8139_io + R_RBSTART, (uint32_t)(uint64_t)_rx);

    /* Interrupt mask: receive OK + transmit OK */
    outw(rtl8139_io + R_IMR, ISR_ROK | ISR_TOK);

    /* RCR: accept physical + multicast + broadcast, 8 KB ring, unlimited DMA */
    outl(rtl8139_io + R_RCR, 0x0000000E | (7u << 8));

    /* TCR: unlimited DMA burst */
    outl(rtl8139_io + R_TCR, (7u << 8));

    /* Enable TX + RX */
    outb(rtl8139_io + R_CR, CR_TE | CR_RE);

    /* Read MAC address from EEPROM shadow registers */
    for (int i = 0; i < 6; i++)
        net_mac.b[i] = inb(rtl8139_io + R_MAC0 + i);

    _rx_ptr = 0;
    return 1;
}

/* ── Send ─────────────────────────────────────────────────── */
int rtl8139_send(const uint8_t *data, uint16_t len) {
    if (!rtl8139_io || len > TX_BUF_SIZE) return -1;

    int d = _tx_cur & (TX_DESC_CNT - 1);

    /* Copy into aligned TX buffer */
    for (int i = 0; i < len; i++) _tx[d][i] = data[i];

    /* Physical address of TX buffer */
    outl(rtl8139_io + R_TSAD0 + d * 4, (uint32_t)(uint64_t)_tx[d]);

    /* Length; writing clears OWN bit → starts DMA */
    outl(rtl8139_io + R_TSD0 + d * 4, (uint32_t)len);

    /* Spin until TOK (bit 15) or TUN (bit 14) */
    { int t = 200000;
      while (!(inl(rtl8139_io + R_TSD0 + d * 4) & 0xC000) && t-- > 0); }

    _tx_cur++;
    return 0;
}

/* ── Poll RX ring ─────────────────────────────────────────── */
void rtl8139_poll(void) {
    if (!rtl8139_io) return;

    while (!(inb(rtl8139_io + R_CR) & CR_BUFE)) {
        /* Packet header: [u16 status][u16 total_len][...data...] */
        uint8_t  *hdr    = _rx + _rx_ptr;
        uint16_t  status  = *(uint16_t *)(hdr);
        uint16_t  pkt_len = *(uint16_t *)(hdr + 2);

        /* ROK must be set; sanity-check length */
        if (!(status & 0x01) || pkt_len < 14 + 4 || pkt_len > 1518 + 4) {
            /* reset RX state */
            _rx_ptr = 0;
            outw(rtl8139_io + R_CAPR, (uint16_t)(0xFFFF - 15));
            break;
        }

        /* data_len = pkt_len - 4 CRC bytes */
        uint16_t data_len = (uint16_t)(pkt_len - 4);
        eth_receive(hdr + 4, data_len);

        /* Advance ring pointer (4-byte aligned, wrap within 8 KB) */
        _rx_ptr = (uint16_t)((_rx_ptr + pkt_len + 4 + 3) & ~3u);
        if (_rx_ptr >= RX_BUF_LEN) _rx_ptr = (uint16_t)(_rx_ptr - RX_BUF_LEN);

        /* Tell NIC the new read position (-16 is the hardware quirk) */
        outw(rtl8139_io + R_CAPR, (uint16_t)(_rx_ptr - 16));
    }

    /* Clear handled interrupt bits */
    outw(rtl8139_io + R_ISR, ISR_ROK | ISR_TOK);
}

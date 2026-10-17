/*
 * ehci.c - OpenWindows Enhanced Host Controller Interface (USB 2.0) (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 *
 * Drives the EHCI host controller (EHCI 1.0) through memory-mapped I/O.
 * Queue heads and qTDs are caller-owned structures: the driver only fills
 * them and hands their physical addresses to the controller.
 */

#include "ehci.h"

/* ── Register access helpers ──────────────────────────────────── */

/* Read a 32-bit operational register (byte offset from the op base). */
static uint32_t ehci_op_read(const ehci_ctrl_t *hc, uint32_t off)
{
    return hc->op_base[off / 4u];
}

/* Write a 32-bit operational register (byte offset from the op base). */
static void ehci_op_write(ehci_ctrl_t *hc, uint32_t off, uint32_t value)
{
    hc->op_base[off / 4u] = value;
}

/* Read a single capability byte (CAPLENGTH and friends are byte sized). */
static uint8_t ehci_cap_read8(const ehci_ctrl_t *hc, uint32_t off)
{
    return hc->cap_base[off];
}

/* Assemble a little-endian 32-bit capability register from bytes. */
static uint32_t ehci_cap_read32(const ehci_ctrl_t *hc, uint32_t off)
{
    uint32_t value = (uint32_t)hc->cap_base[off];
    value |= (uint32_t)hc->cap_base[off + 1u] << 8;
    value |= (uint32_t)hc->cap_base[off + 2u] << 16;
    value |= (uint32_t)hc->cap_base[off + 3u] << 24;
    return value;
}

/* Bounded busy wait. The controller polls in microframe-sized steps and
 * this core has no scheduler hook, so a spin loop is used instead. */
static void ehci_delay(uint32_t units)
{
    volatile uint32_t spin = units;
    while (spin != 0u) {
        spin--;
    }
}

/* Poll a register until (value & mask) == done, or the budget runs out. */
static bool ehci_handshake(const ehci_ctrl_t *hc, uint32_t off,
                           uint32_t mask, uint32_t done, uint32_t budget)
{
    while (budget != 0u) {
        if ((ehci_op_read(hc, off) & mask) == done) {
            return true;
        }
        ehci_delay(1u);
        budget--;
    }
    return false;
}

/* Decode a raw PORTSC value into the public status structure. */
static ehci_port_status_t ehci_decode_portsc(uint32_t portsc)
{
    ehci_port_status_t st;

    st.raw = portsc;
    st.connected = (portsc & EHCI_PORT_CONNECT) != 0u;
    st.enabled = (portsc & EHCI_PORT_PE) != 0u;
    st.suspended = (portsc & EHCI_PORT_SUSPEND) != 0u;
    st.resetting = (portsc & EHCI_PORT_RESET) != 0u;
    st.over_current = (portsc & EHCI_PORT_OC) != 0u;
    st.owned_by_companion = (portsc & EHCI_PORT_OWNER) != 0u;

    /* The line-status field reports a low-speed device; otherwise a
     * connected device on this controller is high speed. */
    if (st.connected && ((portsc & EHCI_PORT_LS_MASK) == EHCI_PORT_LS_LOW)) {
        st.speed = EHCI_SPEED_LOW;
    } else if (st.connected) {
        st.speed = EHCI_SPEED_HIGH;
    } else {
        st.speed = EHCI_SPEED_FULL;
    }
    return st;
}

/* ── Public API ───────────────────────────────────────────────── */

bool ehci_init(ehci_ctrl_t *hc, volatile uint8_t *mmio_base)
{
    uint32_t cmd;

    if ((hc == NULL) || (mmio_base == NULL)) {
        return false;
    }

    hc->cap_base = mmio_base;
    hc->caplength = ehci_cap_read8(hc, EHCI_CAP_CAPLENGTH);
    hc->op_base = (volatile uint32_t *)(void *)(mmio_base + hc->caplength);
    hc->hcsparams = ehci_cap_read32(hc, EHCI_CAP_HCSPARAMS);
    hc->hccparams = ehci_cap_read32(hc, EHCI_CAP_HCCPARAMS);
    hc->num_ports = EHCI_HCS_N_PORTS(hc->hcsparams);
    hc->running = false;
    hc->initialized = true;

    if ((hc->num_ports == 0u) || (hc->num_ports > EHCI_MAX_PORTS)) {
        hc->initialized = false;
        return false;
    }

    /* Full controller reset leaves all operational registers at reset
     * defaults (USBCMD.ITC = 0, USBCFG = 0). */
    if (!ehci_reset(hc)) {
        hc->initialized = false;
        return false;
    }

    /* Single 64-bit address space: no high segment. */
    ehci_op_write(hc, EHCI_OP_CTRLDSSEGMENT, 0u);

    /* Run the controller with an 8 uframe interrupt threshold. The async
     * schedule is enabled later, once a queue head has been queued. */
    cmd = ehci_op_read(hc, EHCI_OP_USBCMD);
    cmd &= ~(EHCI_CMD_RESET | EHCI_CMD_ASE | EHCI_CMD_PSE | EHCI_CMD_IAAD);
    cmd |= EHCI_CMD_RUN | EHCI_CMD_ITC_8;
    ehci_op_write(hc, EHCI_OP_USBCMD, cmd);

    /* Mask interrupts: this driver polls status instead. */
    ehci_op_write(hc, EHCI_OP_USBINTR, 0u);

    /* Claim all root ports from the companion controllers. */
    ehci_op_write(hc, EHCI_OP_CONFIGFLAG, EHCI_CF_PORT_ROUTE);

    if (!ehci_handshake(hc, EHCI_OP_USBSTS, EHCI_STS_HALT, 0u,
                        EHCI_DEFAULT_TIMEOUT)) {
        hc->initialized = false;
        return false;
    }

    hc->running = true;
    return true;
}

bool ehci_reset(ehci_ctrl_t *hc)
{
    uint32_t cmd;

    if ((hc == NULL) || (hc->op_base == NULL)) {
        return false;
    }

    /* Quiesce the schedules and wait for the controller to halt. */
    cmd = ehci_op_read(hc, EHCI_OP_USBCMD);
    cmd &= ~(EHCI_CMD_RUN | EHCI_CMD_ASE | EHCI_CMD_PSE);
    ehci_op_write(hc, EHCI_OP_USBCMD, cmd);

    if (!ehci_handshake(hc, EHCI_OP_USBSTS, EHCI_STS_HALT, EHCI_STS_HALT,
                        EHCI_DEFAULT_TIMEOUT)) {
        return false;
    }

    /* Assert USBCMD.HCRESET and wait for the controller to self-clear it. */
    cmd = ehci_op_read(hc, EHCI_OP_USBCMD);
    ehci_op_write(hc, EHCI_OP_USBCMD, cmd | EHCI_CMD_RESET);
    ehci_delay(EHCI_RESET_HOLD);

    if (!ehci_handshake(hc, EHCI_OP_USBCMD, EHCI_CMD_RESET, 0u,
                        EHCI_DEFAULT_TIMEOUT)) {
        return false;
    }

    hc->running = false;
    return true;
}

bool ehci_poll_port(ehci_ctrl_t *hc, uint8_t port, ehci_port_status_t *status_out)
{
    uint32_t off;
    uint32_t portsc;

    if ((hc == NULL) || !hc->initialized) {
        return false;
    }
    if ((port == 0u) || (port > hc->num_ports)) {
        return false;
    }

    off = EHCI_OP_PORTSC + ((uint32_t)(port - 1u) * 4u);
    portsc = ehci_op_read(hc, off);

    /* Write-1-to-clear the change bits so a single event is reported once. */
    if ((portsc & EHCI_PORT_RWC) != 0u) {
        ehci_op_write(hc, off, portsc | EHCI_PORT_RWC);
        portsc = ehci_op_read(hc, off);
    }

    /* A connected but disabled port needs a reset before it can be used. */
    if (((portsc & EHCI_PORT_CONNECT) != 0u) &&
        ((portsc & EHCI_PORT_PE) == 0u) &&
        ((portsc & EHCI_PORT_OWNER) == 0u)) {
        if ((portsc & EHCI_PORT_RESET) == 0u) {
            ehci_op_write(hc, off,
                          (portsc & ~EHCI_PORT_RWC) | EHCI_PORT_RESET);
            ehci_delay(EHCI_RESET_HOLD);
            portsc = ehci_op_read(hc, off);
        }
        /* Deassert reset and wait for the enable/disable change. */
        ehci_op_write(hc, off, portsc & ~(EHCI_PORT_RWC | EHCI_PORT_RESET));
        (void)ehci_handshake(hc, off, EHCI_PORT_RESET, 0u,
                             EHCI_DEFAULT_TIMEOUT);
        portsc = ehci_op_read(hc, off);
    }

    if (status_out != NULL) {
        *status_out = ehci_decode_portsc(portsc);
    }
    return true;
}

bool ehci_submit_urb(ehci_ctrl_t *hc, const ehci_urb_t *urb,
                     ehci_qtd_t *qtd, ehci_qh_t *qh, void *buffer)
{
    uint32_t qtd_addr;
    uint32_t qh_addr;
    uint32_t page;
    uint32_t addr;
    uint32_t token;

    if ((hc == NULL) || !hc->initialized || (urb == NULL) ||
        (qtd == NULL) || (qh == NULL) || (buffer == NULL)) {
        return false;
    }
    if ((urb->device_address > EHCI_MAX_DEVICE_ADDR) ||
        (urb->endpoint > EHCI_MAX_ENDPOINT) ||
        (urb->pid > EHCI_QTD_PID_SETUP)) {
        return false;
    }

    addr = (uint32_t)(uintptr_t)buffer;
    page = addr & 0xFFFFF000u;
    qtd_addr = (uint32_t)(uintptr_t)qtd;
    qh_addr = (uint32_t)(uintptr_t)qh;

    /* ── Fill the qTD ─────────────────────────────────────────── */
    token = EHCI_QTD_STS_ACTIVE
          | EHCI_QTD_PID(urb->pid)
          | EHCI_QTD_CERR(urb->cerr)
          | EHCI_QTD_BYTES(urb->length)
          | EHCI_QTD_IOC;
    if (urb->toggle != 0u) {
        token |= EHCI_QTD_TOGGLE;
    }

    qtd->next = EHCI_LIST_END;      /* last descriptor in the chain */
    qtd->alt_next = EHCI_LIST_END;  /* no alternate short-read target */
    qtd->token = token;
    qtd->buffer[0] = addr;          /* unaligned start of the transfer */
    qtd->buffer[1] = page + 0x1000u;
    qtd->buffer[2] = page + 0x2000u;
    qtd->buffer[3] = page + 0x3000u;
    qtd->buffer[4] = page + 0x4000u;

    /* ── Fill the queue head ──────────────────────────────────── */
    qh->horiz = (qh_addr & ~0x1Fu) | EHCI_Q_TYPE_QH; /* circular schedule */
    qh->ep_char = EHCI_QH_ADDRESS(urb->device_address)
                | EHCI_QH_ENDPT(urb->endpoint)
                | EHCI_QH_SPEED(urb->speed)
                | EHCI_QH_HEAD
                | EHCI_QH_MPL(urb->max_packet)
                | EHCI_QH_RL(4u);
    if (urb->endpoint == 0u) {
        qh->ep_char |= EHCI_QH_CONTROL_EP;   /* control endpoint flag */
    }
    if ((urb->speed == EHCI_SPEED_LOW) || (urb->speed == EHCI_SPEED_FULL)) {
        /* Full/low speed traffic is routed through a transaction translator. */
        qh->ep_caps = EHCI_QH_MULT(1u)
                    | EHCI_QH_HUB_PORT(urb->hub_port)
                    | EHCI_QH_HUB_ADDR(urb->hub_address);
    } else {
        qh->ep_caps = EHCI_QH_MULT(1u);
    }
    qh->current = 0u;
    /* Overlay matches the first descriptor so the controller starts at it. */
    qh->next_qtd = qtd_addr;
    qh->alt_next = EHCI_LIST_END;
    qh->token = token;
    qh->buffer[0] = qtd->buffer[0];
    qh->buffer[1] = qtd->buffer[1];
    qh->buffer[2] = qtd->buffer[2];
    qh->buffer[3] = qtd->buffer[3];
    qh->buffer[4] = qtd->buffer[4];

    /* Expose the ring to the async schedule and enable it. */
    ehci_op_write(hc, EHCI_OP_ASYNCLISTADDR, qh_addr);
    ehci_op_write(hc, EHCI_OP_USBCMD,
                  ehci_op_read(hc, EHCI_OP_USBCMD)
                  | EHCI_CMD_RUN | EHCI_CMD_ASE);

    /* "Doorbell": re-assert run/stop and kick the async advance engine. */
    ehci_op_write(hc, EHCI_OP_USBCMD,
                  (ehci_op_read(hc, EHCI_OP_USBCMD) & ~EHCI_CMD_IAAD)
                  | EHCI_CMD_RUN);
    return true;
}

/* ── Port status helpers ──────────────────────────────────────── */

bool ehci_port_connected(uint32_t portsc)
{
    return (portsc & EHCI_PORT_CONNECT) != 0u;
}

bool ehci_port_enabled(uint32_t portsc)
{
    return (portsc & EHCI_PORT_PE) != 0u;
}

bool ehci_port_reset_active(uint32_t portsc)
{
    return (portsc & EHCI_PORT_RESET) != 0u;
}

bool ehci_port_is_low_speed(uint32_t portsc)
{
    return (portsc & EHCI_PORT_LS_MASK) == EHCI_PORT_LS_LOW;
}

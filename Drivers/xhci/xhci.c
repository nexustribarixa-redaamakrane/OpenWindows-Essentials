/*
 * xhci.c - OpenWindows Extensible Host Controller Interface (USB 3.x) (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 *
 * Drives an xHCI host controller through memory-mapped I/O. Command and
 * transfer rings, the device context base address array and the scratch
 * memory are all owned by the caller; this module only programs registers
 * and builds TRBs.
 */

#include "xhci.h"

/* ── Register access helpers ──────────────────────────────────── */

/* Read a 32-bit operational register (byte offset from the op base). */
static uint32_t xhci_op_read(const xhci_ctrl_t *hc, uint32_t off)
{
    return hc->op_base[off / 4u];
}

/* Write a 32-bit operational register (byte offset from the op base). */
static void xhci_op_write(xhci_ctrl_t *hc, uint32_t off, uint32_t value)
{
    hc->op_base[off / 4u] = value;
}

/* Read a capability register byte. */
static uint8_t xhci_cap_read8(const xhci_ctrl_t *hc, uint32_t off)
{
    return hc->cap_base[off];
}

/* Assemble a little-endian 16-bit capability register from bytes. */
static uint16_t xhci_cap_read16(const xhci_ctrl_t *hc, uint32_t off)
{
    uint16_t value = (uint16_t)hc->cap_base[off];
    value |= (uint16_t)((uint16_t)hc->cap_base[off + 1u] << 8);
    return value;
}

/* Assemble a little-endian 32-bit capability register from bytes. */
static uint32_t xhci_cap_read32(const xhci_ctrl_t *hc, uint32_t off)
{
    uint32_t value = (uint32_t)hc->cap_base[off];
    value |= (uint32_t)hc->cap_base[off + 1u] << 8;
    value |= (uint32_t)hc->cap_base[off + 2u] << 16;
    value |= (uint32_t)hc->cap_base[off + 3u] << 24;
    return value;
}

/* Bounded busy wait; this core has no timer hook to spin on. */
static void xhci_delay(uint32_t units)
{
    volatile uint32_t spin = units;
    while (spin != 0u) {
        spin--;
    }
}

/* Poll a register until (value & mask) == done, or the budget runs out. */
static bool xhci_handshake(const xhci_ctrl_t *hc, uint32_t off,
                           uint32_t mask, uint32_t done, uint32_t budget)
{
    while (budget != 0u) {
        if ((xhci_op_read(hc, off) & mask) == done) {
            return true;
        }
        xhci_delay(1u);
        budget--;
    }
    return false;
}

/* Decode a raw PORTSC value into the public status structure. */
static xhci_port_status_t xhci_decode_portsc(uint32_t portsc)
{
    xhci_port_status_t st;

    st.raw = portsc;
    st.speed = (uint8_t)XHCI_PORTSC_GET_SPEED(portsc);
    st.link_state = (uint8_t)XHCI_PORTSC_GET_PLS(portsc);
    st.connected = (portsc & XHCI_PORTSC_CCS) != 0u;
    st.enabled = (portsc & XHCI_PORTSC_PED) != 0u;
    st.resetting = (portsc & XHCI_PORTSC_PR) != 0u;
    st.over_current = (portsc & XHCI_PORTSC_OCA) != 0u;
    st.powered = (portsc & XHCI_PORTSC_PP) != 0u;
    return st;
}

/* ── Public API ───────────────────────────────────────────────── */

bool xhci_init(xhci_ctrl_t *hc, volatile uint8_t *mmio_base,
               uint64_t dcbaa_address, uint64_t command_ring_address)
{
    if ((hc == NULL) || (mmio_base == NULL)) {
        return false;
    }

    hc->cap_base = mmio_base;
    hc->caplength = xhci_cap_read8(hc, XHCI_CAP_CAPLENGTH);
    hc->hciversion = xhci_cap_read16(hc, XHCI_CAP_HCIVERSION);
    hc->op_base = (volatile uint32_t *)(void *)(mmio_base + hc->caplength);
    hc->hcsparams1 = xhci_cap_read32(hc, XHCI_CAP_HCSPARAMS1);
    hc->hcsparams2 = xhci_cap_read32(hc, XHCI_CAP_HCSPARAMS2);
    hc->hccparams1 = xhci_cap_read32(hc, XHCI_CAP_HCCPARAMS1);

    /* The doorbell array is dword aligned; the runtime set is 32-byte
     * aligned. Both offsets are relative to the MMIO base. */
    hc->db_offset = xhci_cap_read32(hc, XHCI_CAP_DBOFF) & 0xFFFFFFFCu;
    hc->rt_offset = xhci_cap_read32(hc, XHCI_CAP_RTSOFF) & 0xFFFFFFE0u;
    hc->db_base = (volatile uint32_t *)(void *)(mmio_base + hc->db_offset);
    hc->rt_base = (volatile uint32_t *)(void *)(mmio_base + hc->rt_offset);

    hc->max_slots = XHCI_HCS1_MAX_SLOTS(hc->hcsparams1);
    hc->max_intrs = XHCI_HCS1_MAX_INTRS(hc->hcsparams1);
    hc->max_ports = XHCI_HCS1_MAX_PORTS(hc->hcsparams1);
    hc->context_size = XHCI_HCC1_CSZ(hc->hccparams1) ? 64u : 32u;
    hc->running = false;
    hc->initialized = true;

    if ((hc->max_slots == 0u) || (hc->max_ports == 0u)) {
        hc->initialized = false;
        return false;
    }

    if (!xhci_reset(hc)) {
        hc->initialized = false;
        return false;
    }

    /* Device Context Base Address Array: 64-byte aligned. */
    xhci_op_write(hc, XHCI_OP_DCBAAP,
                  (uint32_t)(dcbaa_address & 0xFFFFFFC0u));
    xhci_op_write(hc, XHCI_OP_DCBAAP_HI,
                  (uint32_t)(dcbaa_address >> 32));

    /* Command ring: 64-byte aligned, producer cycle state starts at 1. */
    xhci_op_write(hc, XHCI_OP_CRCR,
                  ((uint32_t)(command_ring_address & 0xFFFFFFC0u)) | XHCI_CRCR_RCS);
    xhci_op_write(hc, XHCI_OP_CRCR_HI,
                  (uint32_t)(command_ring_address >> 32));

    /* Enable every slot the controller was built to support. */
    xhci_op_write(hc, XHCI_OP_CONFIG,
                  XHCI_CONFIG_MAX_SLOTS(hc->max_slots));
    xhci_op_write(hc, XHCI_OP_DNCTRL, 0u);

    /* Run with interrupt and host-system-error reporting enabled. */
    xhci_op_write(hc, XHCI_OP_USBCMD,
                  XHCI_CMD_INTE | XHCI_CMD_HSEE | XHCI_CMD_RS);

    if (!xhci_handshake(hc, XHCI_OP_USBSTS, XHCI_STS_HCH, 0u,
                        XHCI_TIMEOUT)) {
        hc->initialized = false;
        return false;
    }

    hc->running = true;
    return true;
}

bool xhci_reset(xhci_ctrl_t *hc)
{
    if ((hc == NULL) || (hc->op_base == NULL)) {
        return false;
    }

    /* Assert HCRST; the controller clears it when the reset completes. */
    xhci_op_write(hc, XHCI_OP_USBCMD,
                  xhci_op_read(hc, XHCI_OP_USBCMD) | XHCI_CMD_HCRST);
    xhci_delay(XHCI_RESET_HOLD);

    if (!xhci_handshake(hc, XHCI_OP_USBCMD, XHCI_CMD_HCRST, 0u,
                        XHCI_TIMEOUT)) {
        return false;
    }
    /* Wait for the Controller Not Ready flag to drop. */
    if (!xhci_handshake(hc, XHCI_OP_USBSTS, XHCI_STS_CNR, 0u,
                        XHCI_TIMEOUT)) {
        return false;
    }

    hc->running = false;
    return true;
}

bool xhci_poll_port(xhci_ctrl_t *hc, uint8_t port, xhci_port_status_t *status_out)
{
    uint32_t off;
    uint32_t portsc;
    uint32_t pls;

    if ((hc == NULL) || !hc->initialized) {
        return false;
    }
    if ((port == 0u) || (port > hc->max_ports)) {
        return false;
    }

    off = XHCI_PORT_BASE + ((uint32_t)(port - 1u) * XHCI_PORT_STRIDE);
    portsc = xhci_op_read(hc, off);

    /* Acknowledge the write-1-to-clear change bits. */
    if ((portsc & XHCI_PORTSC_RWC) != 0u) {
        xhci_op_write(hc, off, portsc | XHCI_PORTSC_RWC);
        portsc = xhci_op_read(hc, off);
    }

    pls = XHCI_PORTSC_GET_PLS(portsc);
    if (((portsc & XHCI_PORTSC_CCS) != 0u) &&
        ((portsc & XHCI_PORTSC_PED) == 0u) &&
        ((portsc & XHCI_PORTSC_PR) == 0u) &&
        (pls != XHCI_PLS_INACTIVE) &&
        (pls != XHCI_PLS_DISABLED) &&
        (pls != XHCI_PLS_COMPLIANCE)) {
        xhci_op_write(hc, off,
                      (portsc & ~XHCI_PORTSC_RWC) | XHCI_PORTSC_PR);
        portsc = xhci_op_read(hc, off);
    }

    /* Wait for the reset to finish and the link to come back up. */
    if ((portsc & XHCI_PORTSC_PR) != 0u) {
        (void)xhci_handshake(hc, off, XHCI_PORTSC_PR, 0u, XHCI_TIMEOUT);
        portsc = xhci_op_read(hc, off);
    }

    if (status_out != NULL) {
        *status_out = xhci_decode_portsc(portsc);
    }
    return true;
}

bool xhci_submit_trb(xhci_ctrl_t *hc, xhci_ring_t *ring, uint32_t trb_type,
                     uint64_t parameter, uint32_t status, uint32_t flags,
                     uint8_t slot_id, uint8_t endpoint_id, xhci_trb_t *trb_out)
{
    xhci_trb_t *trb;
    uint32_t control;
    uint32_t idx;
    uint64_t ring_addr;

    if ((hc == NULL) || (ring == NULL) || (ring->base == NULL)) {
        return false;
    }
    if (ring->count < XHCI_RING_MIN_TRBS) {
        return false;
    }

    /* Wrap the producer ring: rewrite the reserved Link TRB and toggle the
     * producer cycle state so the controller follows the new generation. */
    if (ring->enqueue >= (ring->count - 1u)) {
        ring_addr = (uint64_t)(uintptr_t)ring->base;
        trb = &ring->base[ring->count - 1u];
        trb->param_lo = (uint32_t)(ring_addr & 0xFFFFFFFFu);
        trb->param_hi = (uint32_t)(ring_addr >> 32);
        trb->status = 0u;
        trb->control = XHCI_TRB_TYPE(XHCI_TRB_LINK)
                     | XHCI_TRB_LINK_TC
                     | XHCI_TRB_INTR_TARGET(ring->target)
                     | (ring->cycle != 0u ? XHCI_TRB_CYCLE : 0u);
        ring->cycle ^= 1u;
        ring->enqueue = 0u;
    }

    idx = ring->enqueue;
    trb = &ring->base[idx];

    control = flags | XHCI_TRB_TYPE(trb_type);
    if (xhci_trb_is_command(trb_type)) {
        control |= XHCI_TRB_SLOT_ID(slot_id);
    } else if ((trb_type >= (uint32_t)XHCI_TRB_NORMAL) &&
               (trb_type <= (uint32_t)XHCI_TRB_NO_OP)) {
        control |= XHCI_TRB_EP_ID(endpoint_id);
    }
    control |= (ring->cycle != 0u) ? XHCI_TRB_CYCLE : 0u;

    trb->param_lo = (uint32_t)(parameter & 0xFFFFFFFFu);
    trb->param_hi = (uint32_t)(parameter >> 32);
    trb->status = status;
    trb->control = control;

    ring->enqueue = idx + 1u;

    if (trb_out != NULL) {
        *trb_out = *trb;
    }

    /* Ring the doorbell so the controller fetches the new TRB. */
    xhci_ring_doorbell(hc, ring->slot, ring->target);
    return true;
}

void xhci_ring_init(xhci_ring_t *ring, xhci_trb_t *base, uint32_t count,
                    uint8_t slot, uint8_t target)
{
    if (ring == NULL) {
        return;
    }
    ring->base = base;
    ring->count = count;
    ring->enqueue = 0u;
    ring->cycle = 1u;       /* fresh rings start with PCS = 1 */
    ring->slot = slot;
    ring->target = target;
    ring->reserved = 0u;
}

void xhci_ring_doorbell(const xhci_ctrl_t *hc, uint8_t slot, uint8_t target)
{
    if ((hc == NULL) || (hc->db_base == NULL)) {
        return;
    }
    /* Doorbell register [slot]: bits 7:0 select the endpoint/target. */
    hc->db_base[slot] = (uint32_t)target;
}

bool xhci_trb_is_command(uint32_t trb_type)
{
    return (trb_type >= (uint32_t)XHCI_TRB_ENABLE_SLOT) &&
           (trb_type <= (uint32_t)XHCI_TRB_NO_OP_CMD);
}

/* ── Slot context helpers ─────────────────────────────────────── */

void xhci_slot_ctx_init(xhci_slot_ctx_t *ctx, uint8_t speed, uint8_t entries)
{
    uint8_t i;

    if (ctx == NULL) {
        return;
    }
    ctx->dword0 = XHCI_SLOT_SPEED(speed) | XHCI_SLOT_ENTRIES(entries);
    ctx->dword1 = 0u;
    ctx->dword2 = 0u;
    ctx->dword3 = 0u;
    for (i = 0u; i < 4u; i++) {
        ctx->reserved[i] = 0u;
    }
}

void xhci_slot_ctx_set_address(xhci_slot_ctx_t *ctx, uint8_t address)
{
    if (ctx == NULL) {
        return;
    }
    ctx->dword3 = (ctx->dword3 & ~0xFFu) | XHCI_SLOT_DEV_ADDR(address);
}

void xhci_slot_ctx_set_state(xhci_slot_ctx_t *ctx, uint8_t state)
{
    if (ctx == NULL) {
        return;
    }
    ctx->dword3 = (ctx->dword3 & ~(0x1Fu << 27)) | XHCI_SLOT_STATE(state);
}

void xhci_slot_ctx_set_root_port(xhci_slot_ctx_t *ctx, uint8_t port)
{
    if (ctx == NULL) {
        return;
    }
    ctx->dword1 = (ctx->dword1 & ~(0xFFu << 16)) | XHCI_SLOT_ROOT_PORT(port);
}

void xhci_slot_ctx_set_route(xhci_slot_ctx_t *ctx, uint32_t route)
{
    if (ctx == NULL) {
        return;
    }
    ctx->dword0 = (ctx->dword0 & ~0xFFFFFu) | XHCI_SLOT_ROUTE(route);
}

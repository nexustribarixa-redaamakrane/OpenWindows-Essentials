/*
 * owsock64.h - OpenWindows Netwerk Transport & Port Abstraction (.owd)
 *
 * Implements non-POSIX transport endpoints for OpenWindows network services
 * with BANcode status codes and NT-like asynchronous port contracts.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef OWSOCK64_H
#define OWSOCK64_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define OW_SUCCESS              0x00000000U
#define OW_ERR_INVALID_PARAM    0x0011AEE0U
#define OW_ERR_NULL_POINTER     0x0011AEE1U
#define OW_ERR_NOT_FOUND        0x0011AEE2U
#define OW_ERR_INSUFFICIENT     0x0011AEE4U
#define OW_ERR_NETWORK_DOWN     0x0011AEE6U
#define OW_ERR_CONN_REFUSED     0x0011AEE7U
#define OW_ERR_TIMED_OUT        0x0011AEE8U

typedef uint32_t OW_STATUS;
typedef uint32_t OW_ENDPOINT_HANDLE;

#define OW_MAX_ENDPOINTS        16U
#define OW_ENDPOINT_INVALID     0xFFFFFFFFU

typedef enum _OW_TRANSPORT_PROTOCOL {
    OW_PROTO_TCP = 1,
    OW_PROTO_UDP = 2,
    OW_PROTO_RAW = 3
} OW_TRANSPORT_PROTOCOL;

typedef enum _OW_ENDPOINT_STATE {
    OW_EP_CLOSED = 0,
    OW_EP_BOUND = 1,
    OW_EP_LISTENING = 2,
    OW_EP_CONNECTING = 3,
    OW_EP_CONNECTED = 4
} OW_ENDPOINT_STATE;

typedef struct _OW_IPV4_ADDRESS {
    uint8_t     Octets[4];
} OW_IPV4_ADDRESS;

typedef struct _OW_ENDPOINT_DESCRIPTOR {
    bool                    InUse;
    OW_ENDPOINT_STATE       State;
    OW_TRANSPORT_PROTOCOL   Protocol;
    uint16_t                LocalPort;
    uint16_t                RemotePort;
    OW_IPV4_ADDRESS         LocalAddress;
    OW_IPV4_ADDRESS         RemoteAddress;
} OW_ENDPOINT_DESCRIPTOR;

/* ── Public API ───────────────────────────────────────────────────── */

OW_STATUS OwSocketInitialize(void);

OW_STATUS OwSocketCreate(OW_TRANSPORT_PROTOCOL Protocol, OW_ENDPOINT_HANDLE *OutHandle);

OW_STATUS OwSocketBind(OW_ENDPOINT_HANDLE Handle, uint16_t Port);

OW_STATUS OwSocketConnect(OW_ENDPOINT_HANDLE Handle, OW_IPV4_ADDRESS RemoteAddr, uint16_t RemotePort);

OW_STATUS OwSocketSend(OW_ENDPOINT_HANDLE Handle, const void *Buffer, uint32_t Length, uint32_t *BytesSent);

OW_STATUS OwSocketReceive(OW_ENDPOINT_HANDLE Handle, void *Buffer, uint32_t MaxLength, uint32_t *BytesReceived);

OW_STATUS OwSocketClose(OW_ENDPOINT_HANDLE Handle);

#endif /* OWSOCK64_H */

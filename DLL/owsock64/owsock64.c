/*
 * owsock64.c - OpenWindows Netwerk Transport & Port Abstraction Implementation
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "owsock64.h"

static OW_ENDPOINT_DESCRIPTOR g_Endpoints[OW_MAX_ENDPOINTS];
static bool g_SocketInitialized = false;

static void ow_memzero(void *dst, size_t size)
{
    volatile uint8_t *d = (volatile uint8_t *)dst;
    while (size--) {
        *d++ = 0u;
    }
}

OW_STATUS OwSocketInitialize(void)
{
    ow_memzero(g_Endpoints, sizeof(g_Endpoints));
    g_SocketInitialized = true;
    return OW_SUCCESS;
}

OW_STATUS OwSocketCreate(OW_TRANSPORT_PROTOCOL Protocol, OW_ENDPOINT_HANDLE *OutHandle)
{
    uint32_t i;
    if (!OutHandle) return OW_ERR_NULL_POINTER;
    if (!g_SocketInitialized) OwSocketInitialize();

    for (i = 0; i < OW_MAX_ENDPOINTS; i++) {
        if (!g_Endpoints[i].InUse) {
            g_Endpoints[i].InUse = true;
            g_Endpoints[i].State = OW_EP_CLOSED;
            g_Endpoints[i].Protocol = Protocol;
            g_Endpoints[i].LocalPort = 0;
            g_Endpoints[i].RemotePort = 0;
            *OutHandle = i;
            return OW_SUCCESS;
        }
    }

    return OW_ERR_INSUFFICIENT;
}

OW_STATUS OwSocketBind(OW_ENDPOINT_HANDLE Handle, uint16_t Port)
{
    if (Handle >= OW_MAX_ENDPOINTS || !g_Endpoints[Handle].InUse) {
        return OW_ERR_INVALID_PARAM;
    }

    g_Endpoints[Handle].LocalPort = Port;
    g_Endpoints[Handle].State = OW_EP_BOUND;
    return OW_SUCCESS;
}

OW_STATUS OwSocketConnect(OW_ENDPOINT_HANDLE Handle, OW_IPV4_ADDRESS RemoteAddr, uint16_t RemotePort)
{
    if (Handle >= OW_MAX_ENDPOINTS || !g_Endpoints[Handle].InUse) {
        return OW_ERR_INVALID_PARAM;
    }

    g_Endpoints[Handle].RemoteAddress = RemoteAddr;
    g_Endpoints[Handle].RemotePort = RemotePort;
    g_Endpoints[Handle].State = OW_EP_CONNECTED;
    return OW_SUCCESS;
}

OW_STATUS OwSocketSend(OW_ENDPOINT_HANDLE Handle, const void *Buffer, uint32_t Length, uint32_t *BytesSent)
{
    if (!Buffer || !BytesSent) return OW_ERR_NULL_POINTER;
    if (Handle >= OW_MAX_ENDPOINTS || !g_Endpoints[Handle].InUse) {
        return OW_ERR_INVALID_PARAM;
    }
    if (g_Endpoints[Handle].State != OW_EP_CONNECTED && g_Endpoints[Handle].Protocol != OW_PROTO_UDP) {
        return OW_ERR_NOT_FOUND;
    }

    *BytesSent = Length;
    return OW_SUCCESS;
}

OW_STATUS OwSocketReceive(OW_ENDPOINT_HANDLE Handle, void *Buffer, uint32_t MaxLength, uint32_t *BytesReceived)
{
    (void)Buffer;
    if (!BytesReceived) return OW_ERR_NULL_POINTER;
    if (Handle >= OW_MAX_ENDPOINTS || !g_Endpoints[Handle].InUse) {
        return OW_ERR_INVALID_PARAM;
    }

    *BytesReceived = 0;
    (void)MaxLength;
    return OW_SUCCESS;
}

OW_STATUS OwSocketClose(OW_ENDPOINT_HANDLE Handle)
{
    if (Handle >= OW_MAX_ENDPOINTS || !g_Endpoints[Handle].InUse) {
        return OW_ERR_INVALID_PARAM;
    }

    g_Endpoints[Handle].InUse = false;
    g_Endpoints[Handle].State = OW_EP_CLOSED;
    return OW_SUCCESS;
}

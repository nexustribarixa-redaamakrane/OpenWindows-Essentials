/*
 * owpipe.c - OpenWindows Executive Named Pipe Driver Implementation
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "owpipe.h"

static void ow_memzero(void *dst, size_t size)
{
    volatile uint8_t *d = (volatile uint8_t *)dst;
    while (size--) {
        *d++ = 0u;
    }
}

static void ow_strlcpy(char *dst, const char *src, size_t max_len)
{
    size_t i = 0;
    if (!dst || max_len == 0) return;
    if (src) {
        while (src[i] && i + 1 < max_len) {
            dst[i] = src[i];
            i++;
        }
    }
    dst[i] = '\0';
}

OW_STATUS OwPipeCreate(OW_NAMED_PIPE *Pipe, const char *Name)
{
    if (!Pipe || !Name) return OW_ERR_NULL_POINTER;

    ow_memzero(Pipe, sizeof(OW_NAMED_PIPE));
    ow_strlcpy(Pipe->Name, Name, OW_PIPE_MAX_NAME_LEN);
    Pipe->State = OW_PIPE_STATE_CONNECTED;
    Pipe->IsInitialized = true;

    return OW_SUCCESS;
}

OW_STATUS OwPipeWrite(OW_NAMED_PIPE *Pipe, const uint8_t *Data, uint32_t Length, uint32_t *BytesWritten)
{
    uint32_t written = 0;
    if (!Pipe || !Data || !BytesWritten) return OW_ERR_NULL_POINTER;
    if (!Pipe->IsInitialized || Pipe->State != OW_PIPE_STATE_CONNECTED) {
        return OW_ERR_PIPE_BROKEN;
    }

    while (written < Length && Pipe->Count < OW_PIPE_BUFFER_CAPACITY) {
        Pipe->Buffer[Pipe->Tail] = Data[written++];
        Pipe->Tail = (Pipe->Tail + 1) % OW_PIPE_BUFFER_CAPACITY;
        Pipe->Count++;
    }

    *BytesWritten = written;
    return OW_SUCCESS;
}

OW_STATUS OwPipeRead(OW_NAMED_PIPE *Pipe, uint8_t *Buffer, uint32_t MaxLength, uint32_t *BytesRead)
{
    uint32_t read_bytes = 0;
    if (!Pipe || !Buffer || !BytesRead) return OW_ERR_NULL_POINTER;
    if (!Pipe->IsInitialized) return OW_ERR_PIPE_BROKEN;

    while (read_bytes < MaxLength && Pipe->Count > 0) {
        Buffer[read_bytes++] = Pipe->Buffer[Pipe->Head];
        Pipe->Head = (Pipe->Head + 1) % OW_PIPE_BUFFER_CAPACITY;
        Pipe->Count--;
    }

    *BytesRead = read_bytes;
    return OW_SUCCESS;
}

OW_STATUS OwPipeDisconnect(OW_NAMED_PIPE *Pipe)
{
    if (!Pipe) return OW_ERR_NULL_POINTER;
    Pipe->State = OW_PIPE_STATE_DISCONNECTED;
    return OW_SUCCESS;
}

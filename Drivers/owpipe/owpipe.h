/*
 * owpipe.h - OpenWindows Executive Named Pipe Driver (.owc)
 *
 * Implements non-POSIX NT-style Named Pipe IPC endpoints registered
 * in the Object Manager directory under \\Device\\NamedPipe\\.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef OWPIPE_H
#define OWPIPE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define OW_SUCCESS              0x00000000U
#define OW_ERR_INVALID_PARAM    0x0011AEE0U
#define OW_ERR_NULL_POINTER     0x0011AEE1U
#define OW_ERR_NOT_FOUND        0x0011AEE2U
#define OW_ERR_INSUFFICIENT     0x0011AEE4U
#define OW_ERR_PIPE_BROKEN      0x0011AEEBU

typedef uint32_t OW_STATUS;

#define OW_PIPE_BUFFER_CAPACITY 512U
#define OW_PIPE_MAX_NAME_LEN    48U

typedef enum _OW_PIPE_STATE {
    OW_PIPE_STATE_LISTENING = 0,
    OW_PIPE_STATE_CONNECTED = 1,
    OW_PIPE_STATE_DISCONNECTED = 2
} OW_PIPE_STATE;

typedef struct _OW_NAMED_PIPE {
    char            Name[OW_PIPE_MAX_NAME_LEN];
    OW_PIPE_STATE   State;
    uint32_t        Head;
    uint32_t        Tail;
    uint32_t        Count;
    uint8_t         Buffer[OW_PIPE_BUFFER_CAPACITY];
    bool            IsInitialized;
} OW_NAMED_PIPE;

/* ── Public API ───────────────────────────────────────────────────── */

OW_STATUS OwPipeCreate(OW_NAMED_PIPE *Pipe, const char *Name);

OW_STATUS OwPipeWrite(OW_NAMED_PIPE *Pipe, const uint8_t *Data, uint32_t Length, uint32_t *BytesWritten);

OW_STATUS OwPipeRead(OW_NAMED_PIPE *Pipe, uint8_t *Buffer, uint32_t MaxLength, uint32_t *BytesRead);

OW_STATUS OwPipeDisconnect(OW_NAMED_PIPE *Pipe);

#endif /* OWPIPE_H */

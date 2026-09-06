#ifndef EFI_H
#define EFI_H

#include <stdint.h>
#include <stddef.h>

/* UEFI uses the Microsoft x64 calling convention. w64devkit GCC targets
   Windows by default, so ms_abi is already the default. Define EFIAPI
   explicitly for clarity and for freestanding builds. */
#ifndef EFIAPI
#define EFIAPI __attribute__((ms_abi))
#endif

/* ============================================================================
 * Minimal UEFI type definitions for a freestanding x86_64 EFI application.
 * Only the subset needed by the Modular Bootloader is defined here.
 * ============================================================================ */

typedef unsigned short      CHAR16;
typedef unsigned char       BOOLEAN;
typedef unsigned long long  UINTN;
typedef unsigned long long  UINT64;
typedef unsigned int        UINT32;
typedef unsigned short      UINT16;
typedef unsigned char       UINT8;
typedef signed long long    INT64;
typedef signed int          INT32;
typedef signed short        INT16;
typedef signed char         INT8;
typedef signed long long    INTN;
typedef unsigned long long  EFI_PHYSICAL_ADDRESS;
typedef unsigned long long  EFI_VIRTUAL_ADDRESS;
typedef unsigned long long  EFI_LBA;
typedef UINTN               EFI_STATUS;
typedef void               *EFI_HANDLE;

#define EFIERR(n)       (0x8000000000000000ULL | (n))
#define ENCODE_ERROR(x) EFIERR(x)
#define ENCODE_WARNING(x) (x)

#define EFI_SUCCESS             0
#define EFI_LOAD_ERROR          ENCODE_ERROR(1)
#define EFI_INVALID_PARAMETER   ENCODE_ERROR(2)
#define EFI_UNSUPPORTED         ENCODE_ERROR(3)
#define EFI_BAD_BUFFER_SIZE     ENCODE_ERROR(4)
#define EFI_BUFFER_TOO_SMALL    ENCODE_ERROR(5)
#define EFI_NOT_READY           ENCODE_ERROR(6)
#define EFI_DEVICE_ERROR        ENCODE_ERROR(7)
#define EFI_WRITE_PROTECTED     ENCODE_ERROR(8)
#define EFI_OUT_OF_RESOURCES    ENCODE_ERROR(9)
#define EFI_NOT_FOUND           ENCODE_ERROR(14)
#define EFI_NOT_STARTED         ENCODE_ERROR(22)
#define EFI_ALREADY_STARTED     ENCODE_ERROR(23)
#define EFI_ABORTED             ENCODE_ERROR(21)
#define EFI_SECURITY_VIOLATION  ENCODE_ERROR(26)

#define EFI_ERROR(A)    (((INT64)(A)) < 0)

/* ============================================================================
 * EFI_GUID
 * ============================================================================ */
typedef struct {
    UINT32 Data1;
    UINT16 Data2;
    UINT16 Data3;
    UINT8  Data4[8];
} EFI_GUID;

/* ============================================================================
 * EFI_TABLE_HEADER
 * ============================================================================ */
typedef struct {
    UINT64 Signature;
    UINT32 Revision;
    UINT32 HeaderSize;
    UINT32 CRC32;
    UINT32 Reserved;
} EFI_TABLE_HEADER;

/* ============================================================================
 * Memory types
 * ============================================================================ */
typedef enum {
    EfiReservedMemoryType,
    EfiLoaderCode,
    EfiLoaderData,
    EfiBootServicesCode,
    EfiBootServicesData,
    EfiRuntimeServicesCode,
    EfiRuntimeServicesData,
    EfiConventionalMemory,
    EfiUnusableMemory,
    EfiACPIReclaimMemory,
    EfiACPIMemoryNVS,
    EfiMemoryMaxType
} EFI_MEMORY_TYPE;

typedef enum {
    EfiResetCold,
    EfiResetWarm,
    EfiResetShutdown,
    EfiResetPlatformSpecific
} EFI_RESET_TYPE;

typedef struct {
    UINT32 Type;
    EFI_PHYSICAL_ADDRESS PhysicalStart;
    EFI_VIRTUAL_ADDRESS VirtualStart;
    UINT64 NumberOfPages;
    UINT64 Attribute;
} EFI_MEMORY_DESCRIPTOR;

#define EFI_MEMORY_DESCRIPTOR_VERSION 1

/* ============================================================================
 * Forward declarations (needed before protocol structs that reference these)
 * ============================================================================ */
typedef struct _EFI_SIMPLE_TEXT_INPUT_PROTOCOL EFI_SIMPLE_TEXT_INPUT_PROTOCOL;
typedef struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL;
typedef struct _EFI_BLOCK_IO_PROTOCOL EFI_BLOCK_IO_PROTOCOL;
typedef struct _EFI_GRAPHICS_OUTPUT_PROTOCOL EFI_GRAPHICS_OUTPUT_PROTOCOL;
typedef struct _EFI_SYSTEM_TABLE EFI_SYSTEM_TABLE;
typedef struct _EFI_BOOT_SERVICES EFI_BOOT_SERVICES;
typedef struct _EFI_RUNTIME_SERVICES EFI_RUNTIME_SERVICES;

/* ============================================================================
 * EFI_BOOT_SERVICES
 * ============================================================================ */
struct _EFI_BOOT_SERVICES {
    EFI_TABLE_HEADER Hdr;

    /* Task Priority Services */
    void *RaiseTPL;
    void *RestoreTPL;

    /* Memory Services */
    EFI_STATUS (EFIAPI *AllocatePages)(EFI_MEMORY_TYPE, UINTN, EFI_PHYSICAL_ADDRESS *);
    EFI_STATUS (EFIAPI *FreePages)(EFI_PHYSICAL_ADDRESS, UINTN);
    EFI_STATUS (EFIAPI *GetMemoryMap)(UINTN *, void *, UINTN *, UINTN *, UINT32 *);
    EFI_STATUS (EFIAPI *AllocatePool)(EFI_MEMORY_TYPE, UINTN, void **);
    EFI_STATUS (EFIAPI *FreePool)(void *);

    /* Timer & Event Services */
    void *CreateEvent;
    void *SetTimer;
    void *WaitForEvent;
    void *SignalEvent;
    void *CloseEvent;
    void *CheckEvent;

    /* Protocol Handler Services */
    void *InstallProtocolInterface;
    void *ReinstallProtocolInterface;
    void *UninstallProtocolInterface;
    EFI_STATUS (EFIAPI *HandleProtocol)(EFI_HANDLE, EFI_GUID *, void **);
    void *Reserved;
    void *RegisterProtocolNotify;
    EFI_STATUS (EFIAPI *LocateHandleBuffer)(UINTN, EFI_GUID *, void *, UINTN *, EFI_HANDLE **);
    EFI_STATUS (EFIAPI *LocateProtocol)(EFI_GUID *, void *, void **);
    void *InstallMultipleProtocolInterfaces;
    void *UninstallMultipleProtocolInterfaces;

    /* 32-bit CRC Services */
    void *CalculateCrc32;

    /* Miscellaneous Services */
    EFI_STATUS (EFIAPI *CopyMem)(void *, const void *, UINTN);
    EFI_STATUS (EFIAPI *SetMem)(void *, UINTN, UINT8);
    void *CreateEventEx;
};

/* ============================================================================
 * EFI_RUNTIME_SERVICES
 * ============================================================================ */
struct _EFI_RUNTIME_SERVICES {
    EFI_TABLE_HEADER Hdr;

    /* Time Services */
    EFI_STATUS (EFIAPI *GetTime)(void *, void *);
    void *SetTime;
    void *GetWakeupTime;
    void *SetWakeupTime;

    /* Virtual Memory Services */
    void *SetVirtualAddressMap;
    void *ConvertPointer;

    /* Variable Services */
    void *GetVariable;
    void *GetNextVariableName;
    void *SetVariable;

    /* Miscellaneous Services */
    void *GetNextHighMonotonicCount;
    void (EFIAPI *ResetSystem)(EFI_RESET_TYPE, EFI_STATUS, UINTN, void *);

    /* UEFI 2.0 Capsule Services */
    void *UpdateCapsule;
    void *QueryCapsuleCapabilities;

    /* Miscellaneous UEFI 2.0 Service */
    void *QueryVariableInfo;
};

/* ============================================================================
 * EFI_SIMPLE_TEXT_INPUT_PROTOCOL
 * ============================================================================ */
typedef struct {
    UINT16 ScanCode;
    CHAR16 UnicodeChar;
} EFI_INPUT_KEY;

typedef EFI_STATUS (EFIAPI *EFI_INPUT_READ_KEY)(
    EFI_SIMPLE_TEXT_INPUT_PROTOCOL *This,
    EFI_INPUT_KEY *Key
);

typedef EFI_STATUS (EFIAPI *EFI_INPUT_RESET)(
    EFI_SIMPLE_TEXT_INPUT_PROTOCOL *This,
    BOOLEAN ExtendedVerification
);

struct _EFI_SIMPLE_TEXT_INPUT_PROTOCOL {
    EFI_INPUT_RESET Reset;
    EFI_INPUT_READ_KEY ReadKeyStroke;
    void *WaitForKey;
};

/* Scan codes */
#define SCAN_UP       0x01
#define SCAN_DOWN     0x02
#define SCAN_RIGHT    0x03
#define SCAN_LEFT     0x04
#define SCAN_HOME     0x06
#define SCAN_END      0x0B
#define SCAN_PAGE_UP  0x09
#define SCAN_PAGE_DOWN 0x0D
#define SCAN_ESC      0x17

/* ============================================================================
 * EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL
 * ============================================================================ */
typedef struct {
    INT32 MaxMode;
    INT32 Mode;
    INT32 Attribute;
    INT32 CursorColumn;
    INT32 CursorRow;
    BOOLEAN CursorVisible;
} SIMPLE_TEXT_OUTPUT_MODE;

typedef EFI_STATUS (EFIAPI *EFI_TEXT_RESET)(
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This,
    BOOLEAN ExtendedVerification
);

typedef EFI_STATUS (EFIAPI *EFI_TEXT_STRING)(
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This,
    CHAR16 *String
);

typedef EFI_STATUS (EFIAPI *EFI_TEXT_TEST_STRING)(
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This,
    CHAR16 *String
);

typedef EFI_STATUS (EFIAPI *EFI_TEXT_QUERY_MODE)(
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This,
    UINTN ModeNumber,
    UINTN *Columns,
    UINTN *Rows
);

typedef EFI_STATUS (EFIAPI *EFI_TEXT_SET_MODE)(
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This,
    UINTN ModeNumber
);

typedef EFI_STATUS (EFIAPI *EFI_TEXT_SET_ATTRIBUTE)(
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This,
    UINTN Attribute
);

typedef EFI_STATUS (EFIAPI *EFI_TEXT_CLEAR_SCREEN)(
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This
);

typedef EFI_STATUS (EFIAPI *EFI_TEXT_SET_CURSOR_POSITION)(
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This,
    UINTN Column,
    UINTN Row
);

typedef EFI_STATUS (EFIAPI *EFI_TEXT_ENABLE_CURSOR)(
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This,
    BOOLEAN Visible
);

struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL {
    EFI_TEXT_RESET               Reset;
    EFI_TEXT_STRING              OutputString;
    EFI_TEXT_TEST_STRING         TestString;
    EFI_TEXT_QUERY_MODE          QueryMode;
    EFI_TEXT_SET_MODE            SetMode;
    EFI_TEXT_SET_ATTRIBUTE       SetAttribute;
    EFI_TEXT_CLEAR_SCREEN        ClearScreen;
    EFI_TEXT_SET_CURSOR_POSITION SetCursorPosition;
    EFI_TEXT_ENABLE_CURSOR       EnableCursor;
    SIMPLE_TEXT_OUTPUT_MODE      *Mode;
};

/* Text attribute constants */
#define EFI_BLACK         0x00
#define EFI_BLUE          0x01
#define EFI_GREEN         0x02
#define EFI_CYAN          0x03
#define EFI_RED           0x04
#define EFI_MAGENTA       0x05
#define EFI_BROWN         0x06
#define EFI_LIGHTGRAY     0x07
#define EFI_BRIGHT        0x08
#define EFI_DARKGRAY      0x08
#define EFI_LIGHTBLUE     0x09
#define EFI_LIGHTGREEN    0x0A
#define EFI_LIGHTCYAN     0x0B
#define EFI_LIGHTRED      0x0C
#define EFI_LIGHTMAGENTA  0x0D
#define EFI_YELLOW        0x0E
#define EFI_WHITE         0x0F

#define EFI_BACKGROUND_BLACK     0x00
#define EFI_BACKGROUND_BLUE      0x10
#define EFI_BACKGROUND_GREEN     0x20
#define EFI_BACKGROUND_CYAN      0x30
#define EFI_BACKGROUND_RED       0x40
#define EFI_BACKGROUND_MAGENTA   0x50
#define EFI_BACKGROUND_BROWN     0x60
#define EFI_BACKGROUND_LIGHTGRAY 0x70

#define EFI_TEXT_ATTR(Fg, Bg) ((Fg) | ((Bg) << 4))

/* ============================================================================
 * EFI_BLOCK_IO_PROTOCOL
 * ============================================================================ */
typedef struct {
    UINT32 MediaId;
    BOOLEAN RemovableMedia;
    BOOLEAN MediaPresent;
    BOOLEAN LogicalPartition;
    BOOLEAN ReadOnly;
    BOOLEAN WriteCaching;
    UINT32 BlockSize;
    UINT32 IoAlign;
    EFI_LBA LastBlock;
} EFI_BLOCK_IO_MEDIA;

typedef EFI_STATUS (EFIAPI *EFI_BLOCK_RESET)(
    EFI_BLOCK_IO_PROTOCOL *This,
    BOOLEAN ExtendedVerification
);

typedef EFI_STATUS (EFIAPI *EFI_BLOCK_READ)(
    EFI_BLOCK_IO_PROTOCOL *This,
    UINT32 MediaId,
    EFI_LBA LBA,
    UINTN BufferSize,
    void *Buffer
);

typedef EFI_STATUS (EFIAPI *EFI_BLOCK_WRITE)(
    EFI_BLOCK_IO_PROTOCOL *This,
    UINT32 MediaId,
    EFI_LBA LBA,
    UINTN BufferSize,
    void *Buffer
);

typedef EFI_STATUS (EFIAPI *EFI_BLOCK_FLUSH)(
    EFI_BLOCK_IO_PROTOCOL *This
);

struct _EFI_BLOCK_IO_PROTOCOL {
    UINT64             Revision;
    EFI_BLOCK_IO_MEDIA *Media;
    EFI_BLOCK_RESET    Reset;
    EFI_BLOCK_READ     ReadBlocks;
    EFI_BLOCK_WRITE    WriteBlocks;
    EFI_BLOCK_FLUSH    FlushBlocks;
};

/* ============================================================================
 * EFI_GRAPHICS_OUTPUT_PROTOCOL
 * ============================================================================ */
typedef enum {
    PixelRedGreenBlueReserved8BitPerColor,
    PixelBlueGreenRedReserved8BitPerColor,
    PixelBitMask,
    PixelBltOnly,
    PixelFormatMax
} EFI_GRAPHICS_PIXEL_FORMAT;

typedef struct {
    UINT32 RedMask;
    UINT32 GreenMask;
    UINT32 BlueMask;
    UINT32 ReservedMask;
} EFI_PIXEL_BITMASK;

typedef struct {
    UINT32 Version;
    UINT32 HorizontalResolution;
    UINT32 VerticalResolution;
    EFI_GRAPHICS_PIXEL_FORMAT PixelFormat;
    EFI_PIXEL_BITMASK PixelInformation;
    UINT32 PixelsPerScanLine;
} EFI_GRAPHICS_OUTPUT_MODE_INFORMATION;

typedef struct {
    UINT32 MaxMode;
    UINT32 Mode;
    UINT32 SizeOfInfo;
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *Info;
    EFI_PHYSICAL_ADDRESS FrameBufferBase;
    UINTN FrameBufferSize;
} EFI_GRAPHICS_OUTPUT_MODE;

typedef struct {
    UINT8 Blue;
    UINT8 Green;
    UINT8 Red;
    UINT8 Reserved;
} EFI_GRAPHICS_OUTPUT_BLT_PIXEL;

#define EFI_BLT_VIDEO_FILL      0
#define EFI_BLT_VIDEO_TO_BLT_BUFFER 1
#define EFI_BLT_BUFFER_TO_VIDEO 2
#define EFI_BLT_VIDEO_TO_VIDEO  3

typedef EFI_STATUS (EFIAPI *EFI_GRAPHICS_OUTPUT_PROTOCOL_QUERY_MODE)(
    EFI_GRAPHICS_OUTPUT_PROTOCOL *This,
    UINT32 ModeNumber,
    UINTN *SizeOfInfo,
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION **Info
);

typedef EFI_STATUS (EFIAPI *EFI_GRAPHICS_OUTPUT_PROTOCOL_SET_MODE)(
    EFI_GRAPHICS_OUTPUT_PROTOCOL *This,
    UINT32 ModeNumber
);

typedef EFI_STATUS (EFIAPI *EFI_GRAPHICS_OUTPUT_PROTOCOL_BLT)(
    EFI_GRAPHICS_OUTPUT_PROTOCOL *This,
    EFI_GRAPHICS_OUTPUT_BLT_PIXEL *BltBuffer,
    UINTN BltOperation,
    UINTN SourceX,
    UINTN SourceY,
    UINTN DestinationX,
    UINTN DestinationY,
    UINTN Width,
    UINTN Height,
    UINTN Delta
);

struct _EFI_GRAPHICS_OUTPUT_PROTOCOL {
    EFI_GRAPHICS_OUTPUT_PROTOCOL_QUERY_MODE QueryMode;
    EFI_GRAPHICS_OUTPUT_PROTOCOL_SET_MODE   SetMode;
    EFI_GRAPHICS_OUTPUT_PROTOCOL_BLT        Blt;
    EFI_GRAPHICS_OUTPUT_MODE                *Mode;
};

/* ============================================================================
 * EFI_LOADED_IMAGE_PROTOCOL
 * ============================================================================ */
typedef struct _EFI_LOADED_IMAGE_PROTOCOL EFI_LOADED_IMAGE_PROTOCOL;

struct _EFI_LOADED_IMAGE_PROTOCOL {
    UINT32            Revision;
    EFI_HANDLE        ParentHandle;
    EFI_SYSTEM_TABLE  *SystemTable;
    EFI_HANDLE        DeviceHandle;
    void              *FilePath;
    UINT32            LoadOptionsSize;
    void              *LoadOptions;
    void              *ImageBase;
    UINT64            ImageSize;
    EFI_MEMORY_TYPE   ImageCodeType;
    EFI_MEMORY_TYPE   ImageDataType;
    void              *Unload;
    void              *ExitBootServices;
};

/* ============================================================================
 * EFI_SYSTEM_TABLE
 * ============================================================================ */
struct _EFI_SYSTEM_TABLE {
    EFI_TABLE_HEADER              Hdr;
    CHAR16                        *FirmwareVendor;
    UINT32                        FirmwareRevision;
    EFI_HANDLE                    ConsoleInHandle;
    EFI_SIMPLE_TEXT_INPUT_PROTOCOL *ConIn;
    EFI_HANDLE                    ConsoleOutHandle;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *ConOut;
    EFI_HANDLE                    StandardErrorHandle;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *StdErr;
    EFI_RUNTIME_SERVICES          *RuntimeServices;
    EFI_BOOT_SERVICES             *BootServices;
    UINTN                         NumberOfTableEntries;
    void                          *ConfigurationTable;
};

/* ============================================================================
 * Global externs (defined in efi_entry.c)
 * ============================================================================ */
extern EFI_SYSTEM_TABLE       *gST;
extern EFI_BOOT_SERVICES      *gBS;
extern EFI_RUNTIME_SERVICES   *gRT;
extern EFI_HANDLE             gImageHandle;

extern EFI_GRAPHICS_OUTPUT_PROTOCOL     *gGOP;
extern EFI_BLOCK_IO_PROTOCOL            *gBlockIO;
extern EFI_SIMPLE_TEXT_INPUT_PROTOCOL   *gConIn;
extern EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL  *gConOut;

/* ExitBootServices: obtained from the Loaded Image Protocol in efi_entry.c */
typedef EFI_STATUS (EFIAPI *EFI_EXIT_BOOT_SERVICES)(EFI_HANDLE, UINTN);
extern EFI_EXIT_BOOT_SERVICES           gExitBootServices;

/* ============================================================================
 * Convenience wrappers
 * ============================================================================ */
static inline void EFIAPI efi_reset_cold(void)
{
    gRT->ResetSystem(EfiResetCold, EFI_SUCCESS, 0, NULL);
}

static inline void EFIAPI efi_reset_shutdown(void)
{
    gRT->ResetSystem(EfiResetShutdown, EFI_SUCCESS, 0, NULL);
}

#endif /* EFI_H */

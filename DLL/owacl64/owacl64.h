/*
 * owacl64.h - OpenWindows Security Descriptor & Access Control List (.owd)
 *
 * Implements non-POSIX NT-like Access Control Lists (ACL) composed of
 * Access Control Entries (ACE) evaluated against Security Identifiers (SID).
 *
 * All functions return OW_STATUS with canonical BANcode mapping:
 *   - OW_SUCCESS                 (0x00000000u)
 *   - OW_ERR_INVALID_PARAM       (0x0011AEE0u)
 *   - OW_ERR_NULL_POINTER        (0x0011AEE1u)
 *   - OW_ERR_NOT_FOUND           (0x0011AEE2u)
 *   - OW_ERR_INSUFFICIENT        (0x0011AEE4u)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef OWACL64_H
#define OWACL64_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define OW_SUCCESS              0x00000000U
#define OW_ERR_INVALID_PARAM    0x0011AEE0U
#define OW_ERR_NULL_POINTER     0x0011AEE1U
#define OW_ERR_NOT_FOUND        0x0011AEE2U
#define OW_ERR_ALREADY_EXISTS   0x0011AEE3U
#define OW_ERR_INSUFFICIENT     0x0011AEE4U
#define OW_ERR_ACCESS_DENIED    0x0011AEE5U

typedef uint32_t OW_STATUS;

/* ── Well-known Security Identifiers (SID) ────────────────────────── */

#define OW_SID_SYSTEM           0x00000001U
#define OW_SID_ADMINISTRATOR    0x00000002U
#define OW_SID_KERNEL_SERVICE   0x00000003U
#define OW_SID_INTERACTIVE_USER 0x000003E8U  /* UID 1000 */
#define OW_SID_EVERYONE         0x0000FFFFU

/* ── Access Rights Bitmask (OW_ACCESS_MASK) ──────────────────────── */

#define OW_ACCESS_READ_DATA     0x00000001U
#define OW_ACCESS_WRITE_DATA    0x00000002U
#define OW_ACCESS_APPEND_DATA   0x00000004U
#define OW_ACCESS_EXECUTE       0x00000008U
#define OW_ACCESS_DELETE        0x00010000U
#define OW_ACCESS_READ_CONTROL  0x00020000U
#define OW_ACCESS_WRITE_DAC     0x00040000U
#define OW_ACCESS_WRITE_OWNER   0x00080000U
#define OW_ACCESS_SYNCHRONIZE   0x00100000U

#define OW_GENERIC_READ         (OW_ACCESS_READ_DATA | OW_ACCESS_READ_CONTROL)
#define OW_GENERIC_WRITE        (OW_ACCESS_WRITE_DATA | OW_ACCESS_APPEND_DATA | OW_ACCESS_READ_CONTROL)
#define OW_GENERIC_EXECUTE      (OW_ACCESS_EXECUTE | OW_ACCESS_READ_CONTROL)
#define OW_GENERIC_ALL          (0x001F000FU)

/* ── ACE Types ────────────────────────────────────────────────────── */

typedef enum _OW_ACE_TYPE {
    OW_ACE_TYPE_ACCESS_ALLOWED = 0x00,
    OW_ACE_TYPE_ACCESS_DENIED  = 0x01
} OW_ACE_TYPE;

typedef struct _OW_ACE {
    uint8_t     Type;           /* OW_ACE_TYPE */
    uint8_t     Flags;
    uint16_t    Reserved;
    uint32_t    Sid;            /* Target Principal SID */
    uint32_t    AccessMask;     /* Granted or Denied Rights */
} OW_ACE;

#define OW_ACL_MAX_ACES         16U
#define OW_MAX_SECURITY_DESCS   64U
#define OW_MAX_OBJECT_NAME      48U

typedef struct _OW_ACL {
    uint16_t    AceCount;
    uint16_t    AclSize;
    OW_ACE      Aces[OW_ACL_MAX_ACES];
} OW_ACL;

typedef struct _OW_SECURITY_DESCRIPTOR {
    bool        InUse;
    char        ObjectName[OW_MAX_OBJECT_NAME];
    uint32_t    OwnerSid;
    uint32_t    GroupSid;
    OW_ACL      Dacl;           /* Discretionary ACL */
} OW_SECURITY_DESCRIPTOR;

/* ── API Routines ─────────────────────────────────────────────────── */

OW_STATUS OwAclInitialize(void);

OW_STATUS OwAclCreateSecurityDescriptor(const char *ObjectName,
                                        uint32_t OwnerSid,
                                        uint32_t GroupSid,
                                        OW_SECURITY_DESCRIPTOR **OutDesc);

OW_STATUS OwAclAddAccessAllowedAce(OW_SECURITY_DESCRIPTOR *Desc,
                                  uint32_t PrincipalSid,
                                  uint32_t AccessMask);

OW_STATUS OwAclAddAccessDeniedAce(OW_SECURITY_DESCRIPTOR *Desc,
                                 uint32_t PrincipalSid,
                                 uint32_t AccessMask);

OW_STATUS OwAclAccessCheck(const OW_SECURITY_DESCRIPTOR *Desc,
                           uint32_t CallerSid,
                           uint32_t DesiredAccess,
                           uint32_t *GrantedAccess);

OW_STATUS OwAclFindSecurityDescriptor(const char *ObjectName,
                                     OW_SECURITY_DESCRIPTOR **OutDesc);

#endif /* OWACL64_H */

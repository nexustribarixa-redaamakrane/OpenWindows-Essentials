/*
 * owacl64.c - OpenWindows Security Descriptor & Access Control Implementation
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "owacl64.h"

static OW_SECURITY_DESCRIPTOR g_SecurityTable[OW_MAX_SECURITY_DESCS];
static bool g_AclInitialized = false;

static void ow_memzero(void *dst, size_t size)
{
    volatile uint8_t *d = (volatile uint8_t *)dst;
    while (size--) {
        *d++ = 0u;
    }
}

static bool ow_streq(const char *a, const char *b)
{
    if (!a || !b) return false;
    while (*a && *b) {
        if (*a != *b) return false;
        a++;
        b++;
    }
    return (*a == *b);
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

OW_STATUS OwAclInitialize(void)
{
    ow_memzero(g_SecurityTable, sizeof(g_SecurityTable));
    g_AclInitialized = true;
    return OW_SUCCESS;
}

OW_STATUS OwAclCreateSecurityDescriptor(const char *ObjectName,
                                        uint32_t OwnerSid,
                                        uint32_t GroupSid,
                                        OW_SECURITY_DESCRIPTOR **OutDesc)
{
    uint32_t i;
    if (!ObjectName || !OutDesc) {
        return OW_ERR_NULL_POINTER;
    }
    if (!g_AclInitialized) {
        OwAclInitialize();
    }

    /* Check for duplicate */
    for (i = 0; i < OW_MAX_SECURITY_DESCS; i++) {
        if (g_SecurityTable[i].InUse && ow_streq(g_SecurityTable[i].ObjectName, ObjectName)) {
            return OW_ERR_ALREADY_EXISTS;
        }
    }

    /* Allocate free slot */
    for (i = 0; i < OW_MAX_SECURITY_DESCS; i++) {
        if (!g_SecurityTable[i].InUse) {
            g_SecurityTable[i].InUse = true;
            ow_strlcpy(g_SecurityTable[i].ObjectName, ObjectName, OW_MAX_OBJECT_NAME);
            g_SecurityTable[i].OwnerSid = OwnerSid;
            g_SecurityTable[i].GroupSid = GroupSid;
            g_SecurityTable[i].Dacl.AceCount = 0;
            g_SecurityTable[i].Dacl.AclSize = sizeof(OW_ACL);
            *OutDesc = &g_SecurityTable[i];
            return OW_SUCCESS;
        }
    }

    return OW_ERR_INSUFFICIENT;
}

OW_STATUS OwAclAddAccessAllowedAce(OW_SECURITY_DESCRIPTOR *Desc,
                                  uint32_t PrincipalSid,
                                  uint32_t AccessMask)
{
    OW_ACE *ace;
    if (!Desc) return OW_ERR_NULL_POINTER;
    if (Desc->Dacl.AceCount >= OW_ACL_MAX_ACES) {
        return OW_ERR_INSUFFICIENT;
    }

    ace = &Desc->Dacl.Aces[Desc->Dacl.AceCount++];
    ace->Type = OW_ACE_TYPE_ACCESS_ALLOWED;
    ace->Flags = 0;
    ace->Reserved = 0;
    ace->Sid = PrincipalSid;
    ace->AccessMask = AccessMask;

    return OW_SUCCESS;
}

OW_STATUS OwAclAddAccessDeniedAce(OW_SECURITY_DESCRIPTOR *Desc,
                                 uint32_t PrincipalSid,
                                 uint32_t AccessMask)
{
    OW_ACE *ace;
    if (!Desc) return OW_ERR_NULL_POINTER;
    if (Desc->Dacl.AceCount >= OW_ACL_MAX_ACES) {
        return OW_ERR_INSUFFICIENT;
    }

    ace = &Desc->Dacl.Aces[Desc->Dacl.AceCount++];
    ace->Type = OW_ACE_TYPE_ACCESS_DENIED;
    ace->Flags = 0;
    ace->Reserved = 0;
    ace->Sid = PrincipalSid;
    ace->AccessMask = AccessMask;

    return OW_SUCCESS;
}

OW_STATUS OwAclAccessCheck(const OW_SECURITY_DESCRIPTOR *Desc,
                           uint32_t CallerSid,
                           uint32_t DesiredAccess,
                           uint32_t *GrantedAccess)
{
    uint32_t granted = 0;
    uint16_t i;

    if (!Desc || !GrantedAccess) {
        return OW_ERR_NULL_POINTER;
    }

    /* System and Administrator bypass access checks (UID 0 / System model) */
    if (CallerSid == OW_SID_SYSTEM || CallerSid == OW_SID_ADMINISTRATOR) {
        *GrantedAccess = DesiredAccess;
        return OW_SUCCESS;
    }

    /* Owner has standard implicit rights */
    if (CallerSid == Desc->OwnerSid) {
        granted |= (OW_ACCESS_READ_CONTROL | OW_ACCESS_WRITE_DAC);
    }

    /* Evaluate DACL: Denied ACEs take precedence over Allowed ACEs */
    for (i = 0; i < Desc->Dacl.AceCount; i++) {
        const OW_ACE *ace = &Desc->Dacl.Aces[i];
        if (ace->Sid == CallerSid || ace->Sid == OW_SID_EVERYONE) {
            if (ace->Type == OW_ACE_TYPE_ACCESS_DENIED) {
                if (ace->AccessMask & DesiredAccess) {
                    *GrantedAccess = 0;
                    return OW_ERR_ACCESS_DENIED;
                }
            } else if (ace->Type == OW_ACE_TYPE_ACCESS_ALLOWED) {
                granted |= (ace->AccessMask & DesiredAccess);
            }
        }
    }

    *GrantedAccess = granted;
    if ((granted & DesiredAccess) == DesiredAccess) {
        return OW_SUCCESS;
    }

    return OW_ERR_ACCESS_DENIED;
}

OW_STATUS OwAclFindSecurityDescriptor(const char *ObjectName,
                                     OW_SECURITY_DESCRIPTOR **OutDesc)
{
    uint32_t i;
    if (!ObjectName || !OutDesc) return OW_ERR_NULL_POINTER;
    if (!g_AclInitialized) return OW_ERR_NOT_FOUND;

    for (i = 0; i < OW_MAX_SECURITY_DESCS; i++) {
        if (g_SecurityTable[i].InUse && ow_streq(g_SecurityTable[i].ObjectName, ObjectName)) {
            *OutDesc = &g_SecurityTable[i];
            return OW_SUCCESS;
        }
    }

    return OW_ERR_NOT_FOUND;
}

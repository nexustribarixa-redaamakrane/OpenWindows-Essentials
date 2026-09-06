# OpenWindows Security Model

## Components
- **owacl64** - Access Control Lists
- **owcrypt** - Cryptographic primitives
- **entropy** - Hardware random number generation
- **kext_crypto** - Kernel crypto extension
- **kext_sandbox** - Process sandboxing
- **kext_audit** - Security audit logging
- **pfwall** - Network packet filtering

## User Authentication
- Managed by owlogin / owpasswd
- Sessions tracked by kernel
- Privilege elevation via owsu


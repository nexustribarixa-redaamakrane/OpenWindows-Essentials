# OpenWindows Plaintext Script (.kscript, .trapdef, .kmap) Specification

## 1. Overview
Plaintext kernel scripts provide human-readable configuration for initialization, hardware keymaps, and panic trap routing.

## 2. Format Rules
- Plaintext encoded in UTF-8 or SUTF-8.
- Lines beginning with `#` or `;` are treated as comments and ignored.
- Tokens are whitespace-delimited (spaces, tabs).
- Zero memory allocation is required for tokenization; lines are tokenized in-place using pointer-and-length tuples (`kscript_token_t`).

/*
 * managed_engines.h - engines.owd ABI mirror (Managed Language Engines)
 *
 * A hosted language/component bundle for OpenWindows modules. Instead of
 * one engine per programming language (unbounded), this module defines a
 * *census* - a deterministic 32-bit key for every known language name -
 * and a registry of engine handlers that actually execute programs under
 * those keys:
 *
 *   - Census keys: every language in the world has a key, by construction.
 *     meng_census_key() folds any ASCII name to a stable FNV-1a 32 hash,
 *     so SWIFT, COBOL, Brainfuck, APL, "my niece's Esolang" ... all map to
 *     a valid slot. Any module can register a handler under any census key
 *     to "provide" a language at runtime (manageddl).
 *   - Engine registry: a fixed-size table of engine descriptors keyed by
 *     census key, backed by per-engine "run" handlers. Bundled engines:
 *
 *       poco        - the shared stack kernel / IR that small languages
 *                     lower to (integer + string stack machine with
 *                     globals, control flow and CALL/RET).
 *       managednet  - textual bytecode front-end for the poco kernel
 *                     (assembler + executor pair that run a word stream).
 *       managedcom  - component registry: factories by 4CC, interface
 *                     resolution by IID hash, thunked invoke slots.
 *       manageddl   - dynamic language loading: resolve any census key,
 *                     hot-register third-party engines, run manifests
 *                     through whatever handler owns the key.
 *       managedsh   - shell subset: line commands (set/echo/label/goto/
 *                     halt) with $VARIABLE expansion.
 *       managedpy   - Python-like micro subset (reserved; interpreter
 *                     lowers if/while/def/expressions to poco words).
 *       managedjs   - JavaScript-like micro subset (reserved; closures).
 *       managedlua  - Lua-like micro subset (reserved; lexical scopes).
 *
 * All execution is budgeted and deterministic (no wall clock, no heap;
 * scripts run inside a caller-provided meng_machine_t scratch arena).
 *
 * Conforms to OWD1 binary format (Extensions/owd_format.h).
 * C99 freestanding - <stdint.h>/<stdbool.h>/<stddef.h>/<limits.h> only,
 * no external imports (not even kernel64.owd - scratch is caller-owned).
 */

#ifndef OWE_MANAGED_ENGINES_H
#define OWE_MANAGED_ENGINES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/*  OWD1 binary header constants                                       */
/* ------------------------------------------------------------------ */

#define MENG_LIB_TYPE        OWD_LIBTYPE_HYBRID   /* 0x02 user+kernel  */
#define MENG_TARGET_ARCH     0x02u                /* x86_64            */
#define MENG_ALIGNMENT_LOG2  4u

#define MENG_LIB_NAME        "engines.owd"
#define MENG_LIB_VERSION     "engines.owd 1.0.0"

/* ------------------------------------------------------------------ */
/*  Status codes - BANcode mapped                                      */
/*  Range slot: B+ fatal 0x0011A460, W+ warn 0x0011A880,              */
/*              C+ COMcode 0x0011AC00 (engine/component errors),      */
/*              S+ soft 0x0011AE40.                                   */
/* ------------------------------------------------------------------ */

typedef uint32_t meng_status_t;

#define MENG_OK                          0x00000000u  /* Success        */
/* B+ Fatal */
#define MENG_BAN_SCRATCH                 0x0011A460u  /* Arena exhausted*/
#define MENG_BAN_CORE                    0x0011A461u  /* Kernel fault   */
/* W+ Warning */
#define MENG_WARN_BUDGET                 0x0011A880u  /* Step watchdog  */
#define MENG_WARN_TRUNC                  0x0011A881u  /* Output capped  */
/* C+ COMcode (engine / component errors) */
#define MENG_ERR_UNINITIALIZED           0x0011AC00u  /* Not init yet   */
#define MENG_ERR_BAD_ARG                 0x0011AC01u  /* Bad argument   */
#define MENG_ERR_SHORT_OUT               0x0011AC02u  /* Output too small*/
#define MENG_ERR_DUPLICATE               0x0011AC03u  /* Already present*/
#define MENG_ERR_NOT_FOUND               0x0011AC04u  /* No such key    */
#define MENG_ERR_SYNTAX                  0x0011AC05u  /* Program parse  */
#define MENG_ERR_RUNTIME                 0x0011AC06u  /* Program fault  */
#define MENG_ERR_TRAP                    0x0011AC07u  /* Watchdog fired */
#define MENG_ERR_STACK_OVERFLOW          0x0011AC08u  /* Value stack    */
#define MENG_ERR_DEPENDENCY              0x0011AC09u  /* No handler     */
#define MENG_ERR_TYPE                    0x0011AC0Au  /* Type mismatch  */
#define MENG_ERR_STATE                   0x0011AC0Bu  /* Wrong machine  */
/* S+ Soft */
#define MENG_SOFT_HALT                   0x0011AE40u  /* Script halted  */
#define MENG_SOFT_YIELD                  0x0011AE41u  /* Script yielded */
#define MENG_SOFT_EOF                    0x0011AE42u  /* Script ended   */

/* ------------------------------------------------------------------ */
/*  Engine registry sizing                                             */
/* ------------------------------------------------------------------ */

#define MENG_MAX_ENGINES    16u     /* Fixed registry capacity          */
#define MENG_MAX_NAME       63u     /* Longest census name              */

/* ------------------------------------------------------------------ */
/*  Census keys of the bundled engines (FNV-1a 32 of the lowercase     */
/*  canonical name; deterministic and language-agnostic by design).    */
/* ------------------------------------------------------------------ */

#define MENG_KEY_MANAGEDCOM    0x65BC5B17u
#define MENG_KEY_MANAGEDDL     0x073ABAAEu
#define MENG_KEY_MANAGEDPY     0xF41C87B1u
#define MENG_KEY_MANAGEDJS     0xF2529E85u
#define MENG_KEY_MANAGEDNET    0xBADEA0F9u
#define MENG_KEY_MANAGEDLUA    0x7B209500u
#define MENG_KEY_MANAGEDSH     0x0D1A7075u
#define MENG_KEY_POCO          0x92BF6602u

/* ------------------------------------------------------------------ */
/*  Engine handler convention                                          */
/* ------------------------------------------------------------------ */

struct meng_machine;
typedef struct meng_engine meng_engine_t;

typedef meng_status_t (*meng_engine_run_fn)(struct meng_machine *m,
                                            const uint8_t *manifest,
                                            size_t manifest_len);

struct meng_engine {
    uint32_t          key;          /* Census key this handler owns     */
    const char       *name;         /* Display name (static)            */
    uint16_t          abi_minor;    /* Engine ABI revision              */
    uint16_t          generation;   /* Module-load generation stamp     */
    meng_engine_run_fn run;         /* Manifest executor (may be NULL   */
                                    /* while the slot is reserved)      */
};

/* ------------------------------------------------------------------ */
/*  Execution machine (caller-owned scratch arena)                     */
/* ------------------------------------------------------------------ */

#define MENG_SCRATCH_SIZE   256u    /* Kernel CONCAT / staging arena    */
#define MENG_NUM_BUF_SIZE   32u     /* Number rendering buffer          */

#define MENG_MFLAG_STOP     0x00000001u  /* Machine begged to stop      */
#define MENG_MFLAG_TRUNC    0x00000002u  /* Output hit the cap          */

typedef struct meng_machine {
    uint8_t       *out;             /* Caller output buffer (may NULL)  */
    uint32_t       out_cap;         /* Capacity in bytes                */
    uint32_t       out_len;         /* Bytes actually written           */
    uint64_t       out_total;       /* Bytes attempted (trunc detect)   */
    uint64_t       tick;            /* Virtual monotonic clock          */
    uint32_t       step_budget;     /* Watchdog (counts kernel steps)   */
    uint32_t       steps;           /* Steps consumed                   */
    uint32_t       flags;           /* MENG_MFLAG_*                     */
    uint32_t       generation;      /* Engine load generation           */
    uint8_t        _scratch[MENG_SCRATCH_SIZE];
    uint32_t       _scratch_len;
    char           _num[MENG_NUM_BUF_SIZE];
    void          *user;            /* Opaque host context              */
} meng_machine_t;

/* ------------------------------------------------------------------ */
/*  Lifecycle                                                          */
/* ------------------------------------------------------------------ */

meng_status_t  meng_module_init(void);
meng_status_t  meng_module_shutdown(void);
uint32_t       meng_abi_version(void);
uint16_t       meng_abi_major(void);
uint16_t       meng_abi_minor(void);
const uint8_t *meng_ident(void);
const char    *meng_status_text(meng_status_t st);

/* ------------------------------------------------------------------ */
/*  Census (every-known-language slots, by construction)               */
/* ------------------------------------------------------------------ */

/* Fold any ASCII name (a-z0-9._+-) to its stable census key.
 * Succeeds for every well-formed name; the dot is "who owns the
 * dialect" separators, e.g. "python", "cobol", "brainfuck", "lisp44". */
meng_status_t  meng_census_key(const char *name, uint32_t *out);

/* Resolve a census key back to the canonical bundled-engine display
 * name, when the key is one of the bundled census entries. */
meng_status_t  meng_census_name(uint32_t key, char *buf, size_t cap);

/* Number of named census entries bundled in this build. */
uint32_t       meng_census_count(void);

/* ------------------------------------------------------------------ */
/*  Engine registry / dispatch                                         */
/* ------------------------------------------------------------------ */

meng_status_t  meng_engine_register(meng_engine_t *engine);
meng_status_t  meng_engine_resolve(uint32_t key, meng_engine_t **out);
meng_status_t  meng_engine_dispatch(meng_machine_t *m, uint32_t key,
                                    const uint8_t *manifest,
                                    size_t manifest_len);
uint32_t       meng_engine_count(void);

/* ------------------------------------------------------------------ */
/*  Machine helpers                                                    */
/* ------------------------------------------------------------------ */

void meng_machine_reset(meng_machine_t *m);
meng_status_t meng_machine_out(meng_machine_t *m, const uint8_t *p,
                               size_t n);
meng_status_t meng_machine_out_cstr(meng_machine_t *m, const char *s);
meng_status_t meng_machine_out_char(meng_machine_t *m, char c);
meng_status_t meng_machine_out_int(meng_machine_t *m, int64_t v);
uint32_t      meng_machine_step(meng_machine_t *m);  /* 1 if budget left */
void          meng_machine_trim(meng_machine_t *m);  /* out_len <= cap  */

/* ------------------------------------------------------------------ */
/*  Poco kernel (shared stack-machine IR)                              */
/* ------------------------------------------------------------------ */

#define MENG_K_STACK_CELLS  128u    /* Cells = 2 per pushed value      */
#define MENG_K_GLOBALS      64u     /* int64 global slots              */
#define MENG_K_WORDS        256u    /* Assembled program capacity      */
#define MENG_K_LABELS       64u

enum {
    MENG_OP_NOP = 0,
    MENG_OP_PUSHI,     /* a0: immediate int64                         */
    MENG_OP_PUSHS,     /* a0: src offset, a1: byte length             */
    MENG_OP_DUP2,
    MENG_OP_SWAP2,
    MENG_OP_DROP2,
    MENG_OP_IADD,
    MENG_OP_ISUB,
    MENG_OP_IMUL,
    MENG_OP_IDIV,      /* truncating                                    */
    MENG_OP_IMOD,
    MENG_OP_INEG,
    MENG_OP_CMPEQ,
    MENG_OP_CMPNE,
    MENG_OP_CMPLT,
    MENG_OP_CMPLE,
    MENG_OP_JNZ,       /* a0: target pc; pops number, jump if != 0    */
    MENG_OP_JMP,       /* a0: target pc                                */
    MENG_OP_GLOAD,     /* a0: global slot                            */
    MENG_OP_GSTORE,    /* a0: global slot                            */
    MENG_OP_CALL,      /* a0: target pc; pushes return slot           */
    MENG_OP_RET,       /* pop return slot, jump back                  */
    MENG_OP_CONCAT,    /* concat top two strings via machine scratch  */
    MENG_OP_PRINT,     /* pop value, render to machine out            */
    MENG_OP_HALT
};

typedef struct meng_word {
    uint16_t  op;
    uint16_t  pad;
    int64_t   a0;
    int64_t   a1;
} meng_word_t;

/* Assemble a textual poco program (comments ';' or '#', labels "name:",
 * ops and decimal immediates) into the given word buffer. String
 * literals are decoded into the module's const pool; the pool is stable
 * only until the next assemble, so run each program before assembling
 * the next. */
meng_status_t  meng_kernel_assemble(const uint8_t *src, size_t len,
                                    meng_word_t *words, uint32_t cap,
                                    uint32_t *count);

/* Execute an assembled program against `g` (MENG_K_GLOBALS slots). */
meng_status_t  meng_kernel_run(meng_machine_t *m,
                               const meng_word_t *words, uint32_t nw,
                               int64_t *g, uint32_t ng);

/* Convenience: assemble + run in one call. */
meng_status_t  meng_kernel_exec(meng_machine_t *m, const uint8_t *src,
                                size_t len, int64_t *g, uint32_t ng,
                                uint32_t *pc);

/* ------------------------------------------------------------------ */
/*  managedcom component registry                                      */
/* ------------------------------------------------------------------ */

/* Component identity: a 4CC key + a stable IID (FNV-1a 32 of the
 * interface name). Handlers are plain C thunks, so a component is just
 * a table of exposed callbacks plus an opaque data pointer. */

#define MENG_COM_MAX_FACTORIES  16u
#define MENG_COM_MAX_INSTANCES  16u
#define MENG_COM_MAX_IFACE      8u
#define MENG_COM_CC_MAX         4u

typedef uint32_t meng_com_iid_t;

struct meng_com_instance;    /* tag used by the method / factory thunks */

typedef meng_status_t (*meng_com_method_fn)(struct meng_com_instance *inst,
                                            const int64_t *args,
                                            uint32_t argc,
                                            int64_t *result);

typedef struct meng_com_method {
    meng_com_method_fn   fn;
    void                *ctx;
} meng_com_method_t;

typedef struct meng_com_iface {
    uint32_t              iid;            /* IID hash                  */
    meng_com_method_t     mslots[MENG_COM_MAX_IFACE];
} meng_com_iface_t;

typedef meng_status_t (*meng_com_factory_fn)(const uint8_t ccc[4],
                                             struct meng_com_instance *inst,
                                             uint32_t id);

typedef struct meng_com_factory {
    uint32_t               key;           /* 4CC packed LE             */
    meng_com_factory_fn    create;
    void                  *owner;
} meng_com_factory_t;

typedef struct meng_com_instance {
    uint32_t               id;            /* 1-based handle, 0 = free  */
    uint32_t               factory;       /* 4CC of creating factory   */
    void                  *data;          /* Component data            */
    meng_com_iface_t       ifaces[MENG_COM_MAX_IFACE]; /* resolved set */
    uint32_t               iface_count;
} meng_com_instance_t;

/* Register a factory; the instance's `data` is set by the factory fn. */
meng_status_t meng_com_factory_register(uint32_t key,
                                        meng_com_factory_fn create,
                                        void *owner);
meng_status_t meng_com_factory_lookup(uint32_t key,
                                      meng_com_factory_t **out);
meng_status_t meng_com_create(uint32_t key, const uint8_t ccc[4],
                              uint32_t *instance_id);
meng_status_t meng_com_destroy(uint32_t instance_id);
meng_status_t meng_com_resolve(uint32_t instance_id, uint32_t iid,
                               meng_com_instance_t **out);
/* Invoke method `slot` on iface `iid` of instance `id`. */
meng_status_t meng_com_invoke(uint32_t instance_id, uint32_t iid,
                              uint32_t slot, const int64_t *args,
                              uint32_t argc, int64_t *result);
uint32_t meng_com_factory_count(void);
uint32_t meng_com_instance_count(void);

/* ------------------------------------------------------------------ */
/*  manageddl dynamic loading                                          */
/* ------------------------------------------------------------------ */

/* Load a manifest through whichever engine owns `key` (SEE ALSO
 * meng_engine_dispatch). The manifest is opaque - its interpretation
 * is entirely the engine's. */
meng_status_t meng_dl_load(meng_machine_t *m, uint32_t key,
                           const uint8_t *manifest, size_t manifest_len);

/* Hot-register a third-party engine under any census key. */
meng_status_t meng_dl_register_engine(meng_engine_t *engine);

#ifdef __cplusplus
}
#endif

#endif /* OWE_MANAGED_ENGINES_H */
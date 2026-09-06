/*
 * kconf64.c - OpenWindows Hierarchical Configuration Dynamic Library
 * Implementation (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "kconf64.h"
#include "kconf.h"

static kconf_registry_t g_kconf_registry;
static bool g_kconf_initialized = false;

void kconf64_init(void) {
  if (g_kconf_initialized)
    return;
  kconf_init(&g_kconf_registry);

  /* Populate default system configuration tree */
  kconf_set_str(&g_kconf_registry, "kernel/panic/action", "banhammer");
  kconf_set_str(&g_kconf_registry, "ui/wm/theme", "classic_azure");
  kconf_set_int(&g_kconf_registry, "drivers/sio/baud_rate", 115200);
  kconf_set_int(&g_kconf_registry, "kernel/memory/pool_size", 67108864);

  g_kconf_initialized = true;
}

const char *kconf64_get_string(const char *key, const char *default_val) {
  if (!g_kconf_initialized)
    kconf64_init();
  const kconf_node_t *node = kconf_lookup(&g_kconf_registry, key);
  if (node && node->type == KCONF_TYPE_STRING) {
    return node->val_str;
  }
  return default_val;
}

int64_t kconf64_get_int(const char *key, int64_t default_val) {
  if (!g_kconf_initialized)
    kconf64_init();
  const kconf_node_t *node = kconf_lookup(&g_kconf_registry, key);
  if (node && node->type == KCONF_TYPE_INTEGER) {
    return node->val_int;
  }
  return default_val;
}

bool kconf64_set_string(const char *key, const char *val) {
  if (!g_kconf_initialized)
    kconf64_init();
  return kconf_set_str(&g_kconf_registry, key, val);
}

bool kconf64_set_int(const char *key, int64_t val) {
  if (!g_kconf_initialized)
    kconf64_init();
  return kconf_set_int(&g_kconf_registry, key, val);
}

uint32_t kconf64_count(void) {
  if (!g_kconf_initialized)
    kconf64_init();
  return g_kconf_registry.entry_count;
}

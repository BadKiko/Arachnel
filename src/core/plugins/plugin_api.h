#pragma once

#include "plugin_interface.h"

#include <cstddef>

#if defined(_WIN32)
#  if defined(ARACHNEL_PLUGIN_BUILD)
#    define ARACHNEL_PLUGIN_EXPORT __declspec(dllexport)
#  else
#    define ARACHNEL_PLUGIN_EXPORT __declspec(dllimport)
#  endif
#else
#  define ARACHNEL_PLUGIN_EXPORT __attribute__((visibility("default")))
#endif

/**
 * Host speaks API 4 (JSON catalog boundary).
 * Plugins with apiVersion 2/3 still load (CatalogEntry ABI + sizeof gate).
 * API 4 plugins export arachnel_plugin_catalog_json / _free. CatalogEntry sizeof
 * is optional: match → host may call entryById / detectUpdate; mismatch → load
 * anyway, those DLL-crossing calls stay skipped.
 */
#define ARACHNEL_PLUGIN_API_VERSION 4
#define ARACHNEL_PLUGIN_API_VERSION_MIN 2

extern "C" {

ARACHNEL_PLUGIN_EXPORT int arachnel_plugin_api_version();

/** ABI canary: sizeof(CatalogEntry). Required for API 2/3. Optional for API 4. */
ARACHNEL_PLUGIN_EXPORT int arachnel_plugin_catalog_entry_size();

ARACHNEL_PLUGIN_EXPORT arachnel::core::ISourcePlugin* arachnel_plugin_create(
    const char* plugin_root_utf8);

ARACHNEL_PLUGIN_EXPORT void arachnel_plugin_destroy(arachnel::core::ISourcePlugin* plugin);

/**
 * API 4: serialize plugin->catalog() to UTF-8 JSON.
 * Caller must free *out_utf8 with arachnel_plugin_catalog_json_free.
 * Returns 0 on success.
 */
ARACHNEL_PLUGIN_EXPORT int arachnel_plugin_catalog_json(arachnel::core::ISourcePlugin* plugin,
                                                       char** out_utf8, size_t* out_len);

ARACHNEL_PLUGIN_EXPORT void arachnel_plugin_catalog_json_free(char* p);

} // extern "C"

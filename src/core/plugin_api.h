#pragma once

#include "plugin_interface.h"

#if defined(_WIN32)
#  if defined(ARACHNEL_PLUGIN_BUILD)
#    define ARACHNEL_PLUGIN_EXPORT __declspec(dllexport)
#  else
#    define ARACHNEL_PLUGIN_EXPORT __declspec(dllimport)
#  endif
#else
#  define ARACHNEL_PLUGIN_EXPORT __attribute__((visibility("default")))
#endif

#define ARACHNEL_PLUGIN_API_VERSION 1

extern "C" {

ARACHNEL_PLUGIN_EXPORT int arachnel_plugin_api_version();

ARACHNEL_PLUGIN_EXPORT int arachnel_plugin_catalog_entry_size();

ARACHNEL_PLUGIN_EXPORT arachnel::core::ISourcePlugin* arachnel_plugin_create(const char* plugin_root_utf8);

ARACHNEL_PLUGIN_EXPORT void arachnel_plugin_destroy(arachnel::core::ISourcePlugin* plugin);

} // extern "C"

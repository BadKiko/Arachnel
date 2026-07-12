#include "freetp_plugin.h"
#include "plugin_api.h"

extern "C" {

int arachnel_plugin_api_version()
{
    return ARACHNEL_PLUGIN_API_VERSION;
}

arachnel::core::ISourcePlugin* arachnel_plugin_create(const char* plugin_root_utf8)
{
    const QString root =
        plugin_root_utf8 ? QString::fromUtf8(plugin_root_utf8) : QString();
    return new freetp::FreetpPlugin(root);
}

void arachnel_plugin_destroy(arachnel::core::ISourcePlugin* plugin)
{
    delete plugin;
}

} // extern "C"

# Plugins domain

This domain hosts external source plugins. It owns discovery, shared-library
loading, `.arach` package installation and the stable source-plugin ABI.

## Main types

- `PluginHost` scans roots, loads plugins and dispatches plugin work asynchronously.
- `plugin_interface.h` defines `ISourcePlugin` and shared request/result types.
- `plugin_api.h` defines the exported C ABI version functions.
- `PluginCatalogService` downloads entries from the official plugin catalog.
- `SourcePluginModel` exposes configured and loaded source information to QML.

The host currently speaks API v3 and accepts API v2–v3. API v3 supports the
`owns_download` capability, where a plugin reports download/install progress.

## Boundaries

The host must not implement a source's parser, archive logic, update semantics
or launch conventions. Those are implemented in external plugin repositories.
Core services access a plugin through `ISourcePlugin`, never through a
source-specific type.

The SDK's public headers use flat include names; preserve compatibility when
moving or changing them. See [`plugins/README.md`](../../../plugins/README.md)
and [`docs/PLUGIN_SDK.md`](../../../docs/PLUGIN_SDK.md) for plugin authors.

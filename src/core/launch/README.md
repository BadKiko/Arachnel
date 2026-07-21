# Launch domain

This domain turns an installed `LibraryGame` into a tracked application process.
It consumes launch hints supplied by the source plugin and applies local user
overrides where supported.

## Main types

- `LaunchController` coordinates the QML-facing launch flow.
- `LaunchResolver` turns game state and plugin `LaunchInfo` into a resolved path.
- `ProcessLauncher` starts the resolved executable.
- `ProcessTracker` observes the running child process.

`LaunchController` asks `PluginHost` for `ISourcePlugin::launchInfo`, can invoke
runtime preparation through its hooks, and publishes running-game state to the
façade.

## Boundaries

Executable discovery specific to a source belongs in the plugin. Generic process
execution and tracking remain here. Proton and other platform environment setup
belong to `runtime/`; persistent game settings belong to `library/` and
`settings/`.

Do not add source IDs or installer heuristics to resolver or launcher code.

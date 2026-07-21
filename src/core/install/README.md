# Install domain

This domain coordinates an install session after a user selects catalog content.
It owns generic analysis and state transitions; a source plugin owns the
source-specific extraction, installer invocation and patch rules.

## Main types

- `InstallSessionService` starts, advances, retries and completes sessions.
- `InstallAnalyzer` resolves the appropriate plugin and install plan.
- `InstallKindProbeService` detects and caches a catalog entry's UX install kind.
- `install_analysis` and `install_heuristics` describe generic artifact analysis.
- `install_kind` provides the shared install-kind enum.

`InstallSessionService` receives stores, jobs, settings, `PluginHost` and
runtime support through its constructor. On a completed download it builds an
`InstallContext` and delegates to `ISourcePlugin::installFromDownload`.

## Boundaries

`jobs/` owns transport and job state. `library/` owns the committed installed
game. `runtime/` prepares platform runtime dependencies. This domain must not
learn individual source archive layouts or add `sourceId` conditionals.

Put plugin callbacks and generic session policy here; put source pipeline
details in the external plugin.

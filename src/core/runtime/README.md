# Runtime domain

This domain prepares platform runtime requirements needed before an installed
game can run. It contains common environment support rather than source logic.

## Main types

- `RuntimeDependencyService` ensures required runtime dependencies.
- `RuntimeManifestProbe` reads runtime needs from manifests.
- `RuntimeDepotCatalog` describes available runtime depots.
- `RuntimeContainerManager` manages per-game runtime container data.
- `ProtonManager` discovers, orders and downloads Proton versions.
- `WindowsRunner` builds Windows-specific run environment data.

The install and launch flows call runtime support through services or façade
hooks. Runtime state is configured with `SettingsStore` and reflected to QML by
the façade's Proton forwards.

## Boundaries

This domain does not decide what a particular source installs or which executable
a source exposes. That work belongs to plugins and `launch/`. It also does not
persist game records; `library/` owns those records.

Keep platform conditionals local to runtime/launch implementation rather than
spreading them through catalog or install code.

# Settings domain

This domain owns application preferences, notification history and application
update checking. `SettingsStore` is the persisted settings boundary for core.

## Main types

- `SettingsStore` exposes paths, source configuration and runtime preferences.
- `settings_store_persistence.cpp` reads, migrates and writes settings data.
- `NotificationModel` exposes persistent in-app notifications to QML.
- `AppUpdater` checks for releases and requests installer launch when applicable.

Services receive `SettingsStore` where configuration is needed instead of
reading files themselves. The façade exposes the store and model to QML as
readable application services.

## Boundaries

Library records and job records have their own stores in `library/` and `jobs/`.
Plugin discovery and package data belong to `plugins/`. Keep settings migration
centralized in the persistence implementation so callers use typed accessors.

New user-facing settings require QML, persistence and translations to change
together. Avoid persisting ephemeral UI state unless it is a deliberate setting.

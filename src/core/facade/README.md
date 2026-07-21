# Facade domain

This domain contains the application-facing QML boundary and service wiring.
`CoreController` is registered as `Arachnel.Core`; it exposes stable models,
properties and invokables while delegating work to domain services.

## Main files

- `core_controller.h/.cpp` define the QML singleton and lifecycle.
- `core_controller_p.h` keeps composed state private.
- `core_wiring_services.cpp` constructs services and connects their signals.
- `catalog_facade_*`, `job_facade_*`, `library_facade_*`, `launch_facade.cpp`,
  `plugin_facade.cpp` and `proton_facade.cpp` contain forwarding operations.
- `crash_facade.*` forwards crash-report interactions.

## Rules

Keep this layer thin: translate QML input into a domain call, synchronize public
models/properties, and present domain notices. Put new workflow state and logic
in the domain that owns it, then wire the dependency here.

Do not make QML construct domain services or access stores directly. Do not add
source-specific branches here. If a façade `.cpp` approaches 400 lines, split
the forwarding methods by domain as the existing files do.

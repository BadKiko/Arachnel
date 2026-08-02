# Facade domain

This domain contains the application-facing QML boundary and service wiring.
`CoreController` is registered as `Arachnel.Core`; it exposes stable models,
properties and invokables while delegating work to domain services.

## Main files

- `core_controller.h/.cpp` define the QML singleton and lifecycle.
- `core_controller_p.h` keeps composed state private.
- `core_wiring_services.cpp` constructs services and connects their signals.
- Domain façade TUs live next to their domain (`catalog_facade_*.cpp`,
  `job_facade_*.cpp`, `library_facade_*.cpp`, `launch_facade.cpp`,
  `plugin_facade.cpp`, `proton_facade.cpp`) and hold forwarding operations.
- `crash_facade.*` forwards crash-report interactions.

Façade splits are by concern (presentation / query / install, lifecycle /
manual, sync / ops), not by line-count digit suffixes.

## Rules

Keep this layer thin: translate QML input into a domain call, synchronize public
models/properties, and present domain notices. Put new workflow state and logic
in the domain that owns it, then wire the dependency here.

Do not make QML construct domain services or access stores directly. Do not add
source-specific branches here. If a façade `.cpp` grows large, split forwarding
methods by concern.

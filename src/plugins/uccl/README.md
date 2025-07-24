## UCCL Backend Plugin

This backend implements the NIXL backend interface using the UCCL (Unified Collective Communication Library) transport. It is structured similarly to the Mooncake backend, but all data transfer, memory registration, and connection management are performed using UCCL APIs (see `uccl/p2p/engine.h`).

- Source files: `uccl_backend.h`, `uccl_backend.cpp`, `uccl_plugin.cpp`
- Build: The plugin is built via Meson as `libplugin_Uccl.so` (or static if configured).
- Purpose: Allows NIXL to use UCCL as a backend for collective and point-to-point operations.
